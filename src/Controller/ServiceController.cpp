#include "Arduino.h"
#include "WiFi.h"
#include "HTTPClient.h"
#include <esp_timer.h>
#include <WiFiClientSecure.h>
#include "NTPClient.h"
#include "ServiceController.h"
#include "DeviceController.h"

#include <ArduinoJson.h>

// Root CA bundle embedded via platformio.ini board_build.embed_files.
extern const uint8_t rootca_crt_bundle_start[] asm("_binary_data_cert_x509_crt_bundle_bin_start");

extern const char *firmware; // main.cpp - the RUNNING image's version

// Deliberately matches no catalog board, so a build that bypassed platformio.ini's define is never OTA'd.
#ifndef AGRUMY_BOARD
#define AGRUMY_BOARD "unknown"
#endif

// Which commercial KIT (physical PCB) this image was built for, separate from AGRUMY_BOARD: Board picks the OTA binary, Kit tells the server which relay-hardware capability to look up. Empty on generic chip-target environments.
#ifndef AGRUMY_KIT
#define AGRUMY_KIT ""
#endif

// requestPost()'s Authorization header is built from this file-static, not ServiceRequest.header.
static String apiAuth;

// Masks the middle (first 4 + last 4 visible) so serial debugging can spot an obviously wrong value without ever printing a copy-pasteable secret.
String ServiceController::maskSecret(const String &value)
{
    if (value.length() == 0)
    {
        return value;
    }
    if (value.length() <= 8)
    {
        return "****"; // too short to show 4+4 without exposing the whole thing
    }
    return value.substring(0, 4) + "****...****" + value.substring(value.length() - 4);
}

ServiceData ServiceController::requestPost(JsonDocument jsonBuffer, ServiceRequest service)
{
    ServiceData serviceData;
    String jsonRequest;

    serializeJsonPretty(jsonBuffer, jsonRequest);

    if ((WiFi.status() == WL_CONNECTED))
    {
        HTTPClient http;
        String serviceURL = service.url();
        Serial.println("[Service] POST: " + serviceURL);
        Serial.println("[Service] apiId: " + service.header.apiId); // identifier, not a secret
        Serial.println("[Service] apiKey: " + maskSecret(service.header.apiKey));
        Serial.println("[Service] authKey: " + maskSecret(apiAuth));

        if (service.isHttps)
        {
            static WiFiClientSecure secureClient;
            if (deviceConfig.servicePublicKey.length() > 0)
            {
                // Self-hosted deployment: operator pinned a (often self-signed) cert via the admin UI, so pin exactly that.
                secureClient.setCACert(deviceConfig.servicePublicKey.c_str());
            }
            else
            {
                // Publicly-trusted cert: validate against the embedded CA bundle so CA rotation doesn't force a re-flash.
                // secureClient is static, so clear any CA cert a previous call set (setCACertBundle() doesn't).
                secureClient.setCACert(nullptr);
                secureClient.setCACertBundle(rootca_crt_bundle_start);
            }
            http.begin(secureClient, serviceURL);
        }
        else
        {
            // Plain HTTP transitional path while http:// service points still exist.
            http.begin(serviceURL);
        }
        http.addHeader("Content-Type", "application/json");
        http.addHeader("apiId", service.header.apiId);
        http.addHeader("apiKey", service.header.apiKey);
        http.addHeader("Authorization", apiAuth);

        int httpCode = http.POST(jsonRequest);

        // httpCode is negative on error
        if (httpCode > 0)
        {
            Serial.print("[HTTP] Code: ");
            Serial.println(httpCode);
            serviceData.eventlog.errorCode = httpCode;

            if (httpCode == 200 || httpCode == 201)
            {
                serviceData.eventlog.error = false;
                serviceData.payload = http.getString();
            }
            else
            {
                serviceData.eventlog.error = true;
            }
        }
        else
        {
            Serial.printf("[HTTP] Failed, error: %s\n", http.errorToString(httpCode).c_str());
            Serial.println("[HTTP] Error: Bad request.");
            Serial.println(http.getString());
            serviceData.eventlog.error = true;
            serviceData.eventlog.errorCode = httpCode;
        }

        http.end();
    }
    else
    {
        serviceData.eventlog.errorCode = 1000;
        serviceData.eventlog.errorData = "Wifi not available";
        // Guard against recursing into pushEvent() -> requestPost() -> "still no WiFi" -> pushEvent() forever.
        if (service.endpoint != serviceEndpoint.apiEvent)
        {
            pushEvent(service, "NoInternet", "WiFi not connected");
        }
    }
    return serviceData;
}

// Fire-and-forget: never checks the result or retries, so a failed push doesn't chase itself with another event about its own failure.
void ServiceController::pushEvent(ServiceRequest service, String eventType, String message, int commandId)
{
    service.endpoint = serviceEndpoint.apiEvent;
    service.header.apiKey = ""; // session-auth (apiAuth), same as apiConfig() - not apiKey-auth like Authenticate

    JsonDocument payload;
    payload["EventType"] = eventType;
    payload["Message"] = message;
    if (commandId >= 0)
    {
        payload["CommandId"] = commandId; // links this event back to the specific command row
    }

    Serial.println("[Service] pushEvent: " + eventType + (message.length() > 0 ? " (" + message + ")" : ""));
    requestPost(payload, service);
}

// Ack happens BEFORE execute: a Reboot has no "after" on this same connection to report from.
void ServiceController::processPendingCommand(DeviceConfig& config, ServiceRequest serviceRequest, DeviceController& device)
{
    // Roadmap #157: paired with the "nothing queued" log in apiConfig() - if THIS logs "reached with
    // none present" instead, a new config WAS received but ConfigParser.cpp didn't find a
    // "pendingCommand" key in it, which points at BuildDeviceConfigAsync/GetPendingCommandAsync
    // server-side rather than anything below this line.
    if (!config.pendingCommand.present)
    {
        Serial.println("[Service] processPendingCommand: reached with none present");
        return;
    }

    int commandId = config.pendingCommand.idDeviceCommand;
    int actionType = config.pendingCommand.actionType;

    ServiceRequest ackRequest = serviceRequest;
    ackRequest.endpoint = serviceEndpoint.apiCommandAck;
    ackRequest.header.apiKey = ""; // session-auth, same as apiConfig()/pushEvent()

    JsonDocument ackPayload;
    ackPayload["CommandId"] = commandId;
    Serial.println("[Service] Acking pending command " + String(commandId) + " (actionType=" + String(actionType) + ")");
    requestPost(ackPayload, ackRequest);

    switch (actionType)
    {
    case COMMAND_REBOOT:
        Serial.println("[Service] Executing command " + String(commandId) + ": Reboot");
        device.reboot(); // never returns
        break;

    case COMMAND_FORCE_OTA:
    {
        // "Force" = skip apiConfig()'s normal fwVersion != running-image gate; same device.firmwareUpdate() the regular OTA check uses.
        String fwUrl = config.firmwareUrl;
        String fwSha256 = config.firmwareSha256;
        bool otaHttps = fwUrl.startsWith("https://") || fwUrl.startsWith("HTTPS://");

        if (fwUrl.length() == 0)
        {
            Serial.println("[Service] Command " + String(commandId) + " (ForceOTA): no firmware build available to force");
            pushEvent(serviceRequest, "CommandExecuted", "no firmware build available to force", commandId);
            break;
        }

        if (device.firmwareUpdate(fwUrl, otaHttps, fwSha256))
        {
            Serial.println("[Service] Command " + String(commandId) + " (ForceOTA) succeeded, rebooting into new image");
            pushEvent(serviceRequest, "CommandExecuted", "version=" + config.firmwareVersion, commandId);
            device.reboot(); // never returns
        }

        Serial.println("[Service] Command " + String(commandId) + " (ForceOTA) failed - staying on current firmware");
        pushEvent(serviceRequest, "CommandExecuted", "download/flash failed, version=" + config.firmwareVersion, commandId);
        break;
    }

    case COMMAND_FORCE_CONFIG_SYNC:
        // The config the server just sent in THIS SAME poll response already IS the resync - nothing left to do differently.
        Serial.println("[Service] Command " + String(commandId) + " (ForceConfigSync): config already current from this same poll, nothing further to do");
        pushEvent(serviceRequest, "CommandExecuted", "config already current from this poll", commandId);
        break;

    default:
        Serial.println("[Service] Command " + String(commandId) + ": unknown actionType " + String(actionType) + ", ignoring");
        break;
    }
}

void ServiceController::apiAuthenticate(DeviceConfig deviceConfig, ServiceRequest serviceRequest, DeviceController& device)
{
    Serial.println("[Service] apiAuthentication: ");
    serviceRequest.endpoint = serviceEndpoint.apiAuthenticate;
    serviceRequest.header.apiId = deviceConfig.apiId;
    serviceRequest.header.apiKey = deviceConfig.apiKey;

    ServiceData serviceData;
    JsonDocument payload;
    serviceData = requestPost(payload, serviceRequest);

    // Tolerate a transient auth failure - factory reset only after several consecutive 401s, not the first one.
    static int consecutiveAuthFailures = 0;
    const int MAX_CONSECUTIVE_AUTH_FAILURES = 3;
    if(serviceData.eventlog.errorCode==401){
        consecutiveAuthFailures++;
        Serial.printf("[Service] Device failed authentication (%d/%d consecutive)\n", consecutiveAuthFailures, MAX_CONSECUTIVE_AUTH_FAILURES);
        if (consecutiveAuthFailures >= MAX_CONSECUTIVE_AUTH_FAILURES)
        {
            Serial.println("[Service] Too many consecutive auth failures, reseting device to defaults...");
            // Best-effort: this push will very likely also fail (no valid apiAuth yet) - expected, not special-cased.
            pushEvent(serviceRequest, "AuthFailed", "apiId/apiKey rejected by server, consecutive failures: " + String(consecutiveAuthFailures));
            device.reset();
        }
        return; // no valid payload to parse below on a 401 - avoid setting apiAuth from an error body
    }
    consecutiveAuthFailures = 0;

    DeserializationError error = deserializeJson(payload, serviceData.payload);
    if (error)
    {
        // A truncated/corrupt body must not leave a blank-but-"successful" apiAuth; clear it so the next 401 reflects real state and feeds the failure counter.
        Serial.print("[Service] apiAuthenticate: deserializeJson failed: ");
        Serial.println(error.c_str());
        apiAuth = "";
        return;
    }

    String output = payload["apiAuth"];
    apiAuth = output;
    Serial.println("[Service] apiAuthentication authKey: " + maskSecret(apiAuth));
}

bool ServiceController::apiConfig(DeviceConfig& deviceConfig, ServiceRequest serviceRequest, DeviceController& device)
{
    String configVersion=String(deviceConfig.configVersion);

    Serial.print("[Service] Current configVersion: ");
    Serial.println(configVersion);

    serviceRequest.endpoint = serviceEndpoint.apiConfig;
    serviceRequest.header.apiId = deviceConfig.apiId;
    serviceRequest.header.apiKey = "";

    ServiceData serviceData;
    JsonDocument payload;
    payload["ConfigVersion"] = configVersion;
    // The config poll doubles as the heartbeat. esp_timer, not millis(): 64-bit, no 49-day wrap.
    payload["Uptime"] = (uint32_t)(esp_timer_get_time() / 1000000ULL);
    payload["Rssi"] = WiFi.RSSI();
    payload["FreeHeap"] = ESP.getFreeHeap();
    payload["FirmwareVersion"] = firmware;
    payload["Board"] = AGRUMY_BOARD; // PlatformIO env name from the build flag, never guessed from the chip at runtime
    payload["Kit"] = AGRUMY_KIT;

    serviceData = requestPost(payload, serviceRequest);

    if(serviceData.eventlog.errorCode==401){

        Serial.println("[Service] apiConfig: failed to authenticate: ");
        apiAuthenticate(deviceConfig,serviceRequest, device);
        serviceData = requestPost(payload, serviceRequest);
    }

    // Reboot only after several failed cycles in a row - a reboot clears a fragmented heap but shouldn't fire on one transient hiccup.
    static int consecutiveFailures = 0;
    const int MAX_CONSECUTIVE_CONFIG_FAILURES = 3;
    if(serviceData.eventlog.error){
        consecutiveFailures++;
        Serial.print("[Service] Error accesing service point: ");
        Serial.println(serviceData.eventlog.errorCode);
        Serial.printf("[Service] Consecutive failed config cycles: %d/%d\n", consecutiveFailures, MAX_CONSECUTIVE_CONFIG_FAILURES);
        if (consecutiveFailures >= MAX_CONSECUTIVE_CONFIG_FAILURES)
        {
            Serial.println("[Service] Too many consecutive failures, rebooting.");
            pushEvent(serviceRequest, "ConfigSyncFailed", "consecutive failures: " + String(consecutiveFailures));
            device.reboot();
        }
    } else {
        consecutiveFailures = 0;
    }

    // Derive firmware state from the config about to run - the new one if received, else the boot config (admin may set the flag without bumping configVersion -> 200, no body).
    bool   fwFlag    = deviceConfig.firmwareUpdate;
    String fwVersion = deviceConfig.firmwareVersion;
    String fwUrl     = deviceConfig.firmwareUrl;
    String fwSha256  = deviceConfig.firmwareSha256;

    bool receivedNewConfig = !serviceData.payload.isEmpty();
    DeviceConfig newConfig;

    // Roadmap #157: diagnostic for the "queued command never executes" report - an empty body here
    // means the server decided nothing changed AND nothing is queued (DeviceApiController.GetConfig's
    // own check), so if a command was supposedly issued but this still logs "nothing queued", the bug
    // is server-side, not in this file's dispatch below.
    if (!receivedNewConfig) {
        Serial.println("[Service] apiConfig: no new config this cycle (up to date, nothing queued)");
    }

    if (receivedNewConfig) {
        Serial.println(serviceData.payload);
        // Parse-gate BEFORE persisting - a truncated body must neither clobber config.json nor be applied.
        JsonDocument parseCheck;
        if (deserializeJson(parseCheck, serviceData.payload) != DeserializationError::Ok) {
            Serial.println("[Service] New config payload failed to parse - ignoring it this cycle");
            receivedNewConfig = false;
        } else {
            // loadConfig() gates on required identity keys (apiId/apiKey/servicePoint) and does no disk I/O, so it runs before saveConfigFile.
            newConfig = device.loadConfig(serviceData.payload);
            if (newConfig.eventlog.error) {
                Serial.println("[Service] New config rejected (code " + String(newConfig.eventlog.errorCode) + "): " + newConfig.eventlog.errorData);
                pushEvent(serviceRequest, "ConfigSyncFailed", "code=" + String(newConfig.eventlog.errorCode) + " " + newConfig.eventlog.errorData);
                receivedNewConfig = false;
            } else {
                Serial.println("[Service] New config received, saving new config");
                device.saveConfigFile(serviceData.payload); // backs up the old config.json before overwriting it
                device.waitForFileCommitted("config.json"); // verified, not a bare delay()

                fwFlag    = newConfig.firmwareUpdate;
                fwVersion = newConfig.firmwareVersion;
                fwUrl     = newConfig.firmwareUrl;
                fwSha256  = newConfig.firmwareSha256;

                // Ahead of the regular OTA gate below on purpose: a pending Reboot must fire before anything else this cycle, and a pending ForceOTA gets its own shot even if the version-mismatch gate would otherwise skip it.
                processPendingCommand(newConfig, serviceRequest, device);
            }
        }
    }

    // OTA only when the server asks AND the offered version differs from this image, so a stale flag can't loop forever; do it before the reboot below.
    if (fwFlag && fwUrl.length() > 0 && fwVersion != String(firmware)) {
        bool otaHttps = fwUrl.startsWith("https://") || fwUrl.startsWith("HTTPS://");
        Serial.println("[Service] Firmware update " + fwVersion + " available (running " + String(firmware) + ")");
        if (device.firmwareUpdate(fwUrl, otaHttps, fwSha256)) {
            Serial.println("[Service] OTA succeeded, rebooting into new image");
            device.reboot();
        }
        // failed download: fall through, keep running current firmware, retry next cycle
        Serial.println("[Service] OTA failed - staying on current firmware, will retry next config cycle");
        pushEvent(serviceRequest, "OtaFailed", "version=" + fwVersion);
    }

    if (receivedNewConfig) {
        // Reboot only when a field tied to boot-time state changed (transport/TLS setup, identity, sleep mode); everything else is read from deviceConfig every cycle and applies without a reboot.
        bool rebootRequired =
            newConfig.deviceTypeServiceID != deviceConfig.deviceTypeServiceID ||
            newConfig.servicePoint        != deviceConfig.servicePoint ||
            newConfig.servicePublicKey    != deviceConfig.servicePublicKey ||
            newConfig.apiId               != deviceConfig.apiId ||
            newConfig.apiKey              != deviceConfig.apiKey ||
            newConfig.sleepDeep           != deviceConfig.sleepDeep;

        if (rebootRequired) {
            // Feed the crash-loop-guard counter ONLY here, never on the OTA or too-many-failures reboots, so an unrelated reboot cause never falsely triggers a rollback in setup().
            device.notePendingConfigReboot(millis());
            device.reboot(); // boot into the newly saved config
        }

        deviceConfig = newConfig;
        Serial.println("[Service] Config hot-applied without reboot (version " + String(deviceConfig.configVersion) + ")");
        pushEvent(serviceRequest, "ConfigApplied", "version=" + String(deviceConfig.configVersion));
        return true;
    }

    Serial.println("[Service] Config didn't change, do nothing");
    return false;
}
