#include "Arduino.h"
#include "WiFi.h"
#include "HTTPClient.h"
#include <esp_timer.h>
#include <WiFiClientSecure.h>
#include "NTPClient.h"
#include "ServiceController.h"
#include "DeviceController.h"

#include <ArduinoJson.h>

// Root CA bundle embedded via platformio.ini board_build.embed_files (see roadmap #3 notes).
extern const uint8_t rootca_crt_bundle_start[] asm("_binary_data_cert_x509_crt_bundle_bin_start");

// main.cpp - the RUNNING image's version, reported in the config-poll heartbeat (roadmap #7)
// and compared against OTA offers below.
extern const char *firmware;

static ServiceRequest serviceRequest;
static ServiceEndpoint serviceEndpoint;
// requestPost()'s Authorization header is built from THIS file-static, not ServiceRequest.header -
// ServiceHeader had its own apiAuth field until roadmap #98 removed it: it was written in three
// places but never once read, since every actual Authorization header send already used this
// static (see requestPost() below).
static String apiAuth;

// Roadmap #20: apiKey/authKey are usable to impersonate the device if copied off Serial (confirmed
// live during the #77 physical test) - mask the middle like SQL data masking (first 4 + last 4
// visible) so serial debugging still shows enough to spot an obviously wrong/truncated value,
// without ever printing a value someone reading the monitor could copy-paste as a working secret.
// Blank stays blank (nothing to mask, and most calls legitimately send an empty apiKey/authKey -
// see the session-vs-apiKey-auth comments in apiConfig()/apiAuthenticate() below).
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
        String serviceURL = service.serviceType + service.servicePoint + service.endpoint;
        Serial.println("[Service] POST: " + serviceURL);
        Serial.println("[Service] apiId: " + service.header.apiId); // identifier, not a secret - see roadmap #73
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
        // Guard against recursing into pushEvent() -> requestPost() -> "still no WiFi" -> pushEvent()
        // forever: only fire if THIS request isn't itself the event push (pushEvent() rewrites
        // .endpoint to apiEvent before calling back in, so the second pass trips this and stops).
        // Reporting "NoInternet" while there genuinely is none can never actually reach the server -
        // this attempt fails the same way and is a harmless no-op, not a bug.
        if (service.endpoint != serviceEndpoint.apiEvent)
        {
            pushEvent(service, "NoInternet", "WiFi not connected");
        }
    }
    return serviceData;
}

// Roadmap #28: fire-and-forget event push, reusing whatever apiId/serviceType/servicePoint the
// caller's ServiceRequest already carries. Never checks the result and never retries - a failed
// event push (no internet, stale apiAuth, etc.) must stay silent, not chase itself with another
// event about its own failure.
void ServiceController::pushEvent(ServiceRequest service, String eventType, String message)
{
    service.endpoint = serviceEndpoint.apiEvent;
    service.header.apiKey = ""; // session-auth (apiAuth), same as apiConfig() - not apiKey-auth like Authenticate

    JsonDocument payload;
    payload["EventType"] = eventType;
    payload["Message"] = message;

    Serial.println("[Service] pushEvent: " + eventType + (message.length() > 0 ? " (" + message + ")" : ""));
    requestPost(payload, service);
}

// API requests
void ServiceController::apiAuthenticate(DeviceConfig deviceConfig, ServiceRequest serviceRequest, DeviceController& device)
{
    Serial.println("[Service] apiAuthentication: ");
    serviceRequest.endpoint = serviceEndpoint.apiAuthenticate;
    serviceRequest.header.apiId = deviceConfig.apiId;
    serviceRequest.header.apiKey = deviceConfig.apiKey;

    ServiceData serviceData;
    JsonDocument payload;
    serviceData = requestPost(payload, serviceRequest);

    // Roadmap #97: tolerate a transient auth failure (network blip, server restart mid-request)
    // the same way apiConfig() already tolerates a transient config-sync failure - factory reset
    // only after several consecutive failures, not on the very first one. Same counter-before-
    // terminal-action shape as apiConfig()'s consecutiveFailures/MAX_CONSECUTIVE_CONFIG_FAILURES,
    // applied here to specifically-401 (a genuinely rejected apiId/apiKey, not just any error).
    static int consecutiveAuthFailures = 0;
    const int MAX_CONSECUTIVE_AUTH_FAILURES = 3;
    if(serviceData.eventlog.errorCode==401){
        consecutiveAuthFailures++;
        Serial.printf("[Service] Device failed authentication (%d/%d consecutive)\n", consecutiveAuthFailures, MAX_CONSECUTIVE_AUTH_FAILURES);
        if (consecutiveAuthFailures >= MAX_CONSECUTIVE_AUTH_FAILURES)
        {
            Serial.println("[Service] Too many consecutive auth failures, reseting device to defaults...");
            // Best-effort: this push will very likely also fail (no valid apiAuth yet, that's exactly
            // what just failed) - expected and acceptable, not something to special-case.
            pushEvent(serviceRequest, "AuthFailed", "apiId/apiKey rejected by server, consecutive failures: " + String(consecutiveAuthFailures));
            device.reset();
        }
        return; // no valid payload to parse below on a 401 - avoid setting apiAuth from an error body
    }
    consecutiveAuthFailures = 0;

    Serial.print("[Heap] before deserializeJson (apiAuthenticate): "); // TEMPORARY DIAGNOSTIC (roadmap #3)
    Serial.println(ESP.getFreeHeap());

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
    // Roadmap #7: the config poll doubles as the heartbeat - see contracts/device-api/
    // config.request.schema.json. esp_timer, not millis(): 64-bit, no 49-day wrap.
    payload["Uptime"] = (uint32_t)(esp_timer_get_time() / 1000000ULL);
    payload["Rssi"] = WiFi.RSSI();
    payload["FreeHeap"] = ESP.getFreeHeap();
    payload["FirmwareVersion"] = firmware;

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

    // roadmap #3 (OTA): derive firmware state from the config about to run - the new one if received, else the boot config (admin may set the flag without bumping configVersion -> 200, no body).
    bool   fwFlag    = deviceConfig.firmwareUpdate;
    String fwVersion = deviceConfig.firmwareVersion;
    String fwUrl     = deviceConfig.firmwareUrl;

    bool receivedNewConfig = !serviceData.payload.isEmpty();
    DeviceConfig newConfig;

    if (receivedNewConfig) {
        Serial.println(serviceData.payload);
        // Roadmap #67: parse-gate BEFORE persisting - a truncated body must neither clobber
        // config.json nor be applied; keep running on the current config, the server keeps
        // offering the new one on every poll while our configVersion is behind.
        JsonDocument parseCheck;
        if (deserializeJson(parseCheck, serviceData.payload) != DeserializationError::Ok) {
            Serial.println("[Service] New config payload failed to parse - ignoring it this cycle");
            receivedNewConfig = false;
        } else {
            // Roadmap #107: loadConfig() now gates on required identity keys (apiId/apiKey/
            // servicePoint) - run it BEFORE saveConfigFile so a valid-but-incomplete payload
            // (contract drift, e.g. "{}") never reaches disk, same "validate before persisting"
            // principle as the #67 parse-gate above. loadConfig() does no disk I/O itself, so this
            // reorder changes nothing about what it parses.
            newConfig = device.loadConfig(serviceData.payload);
            if (newConfig.eventlog.error) {
                Serial.println("[Service] New config rejected (code " + String(newConfig.eventlog.errorCode) + "): " + newConfig.eventlog.errorData);
                pushEvent(serviceRequest, "ConfigSyncFailed", "code=" + String(newConfig.eventlog.errorCode) + " " + newConfig.eventlog.errorData);
                receivedNewConfig = false;
            } else {
                Serial.println("[Service] New config received, saving new config");
                device.saveConfigFile(serviceData.payload); // backs up the old config.json before overwriting it (config integrity)
                delay(1000); // let the write settle

                fwFlag    = newConfig.firmwareUpdate;
                fwVersion = newConfig.firmwareVersion;
                fwUrl     = newConfig.firmwareUrl;
            }
        }
    }

    // OTA only when the server asks AND the offered version differs from this image, so a stale flag can't loop forever; do it before the reboot below.
    if (fwFlag && fwUrl.length() > 0 && fwVersion != String(firmware)) {
        bool otaHttps = fwUrl.startsWith("https://") || fwUrl.startsWith("HTTPS://");
        Serial.println("[Service] Firmware update " + fwVersion + " available (running " + String(firmware) + ")");
        if (device.firmwareUpdate(fwUrl, otaHttps)) {
            Serial.println("[Service] OTA succeeded, rebooting into new image");
            device.reboot();
        }
        // failed download: fall through, keep running current firmware, retry next cycle
        Serial.println("[Service] OTA failed - staying on current firmware, will retry next config cycle");
        pushEvent(serviceRequest, "OtaFailed", "version=" + fwVersion);
    }

    if (receivedNewConfig) {
        // Roadmap #67: reboot only when a field tied to boot-time state changed - transport
        // (deviceTypeServiceID/servicePoint/servicePublicKey: static WiFiClientSecure TLS state,
        // serviceRequest derived once in setup()), identity (apiId/apiKey: safer rotated through
        // a clean restart), sleepDeep (a property of the boot/sleep cycle itself). Everything
        // else - thresholds, hysteresis, intervals, relay assignments, the enable flags - is
        // read from deviceConfig every cycle and takes effect immediately without a reboot.
        bool rebootRequired =
            newConfig.deviceTypeServiceID != deviceConfig.deviceTypeServiceID ||
            newConfig.servicePoint        != deviceConfig.servicePoint ||
            newConfig.servicePublicKey    != deviceConfig.servicePublicKey ||
            newConfig.apiId               != deviceConfig.apiId ||
            newConfig.apiKey              != deviceConfig.apiKey ||
            newConfig.sleepDeep           != deviceConfig.sleepDeep;

        if (rebootRequired) {
            // Config-integrity crash-loop guard: feed the counter ONLY here, right before this
            // specific reboot - never on the OTA reboot above or the too-many-failures reboot,
            // so an unrelated reboot cause never falsely triggers a rollback in setup().
            device.notePendingConfigReboot(millis());
            device.reboot(); // boot into the newly saved config
        }

        // Hot-apply: update the caller's instance through the reference and confirm to the
        // server now - the boot-time ConfigApplied path (#37) never runs because there is no
        // reboot. #62's crash-loop rollback deliberately stays scoped to reboot-applied configs:
        // the fields that can reach this branch are re-read every cycle and cannot wedge the
        // boot path, so arming the counter here would only risk false rollbacks.
        deviceConfig = newConfig;
        Serial.println("[Service] Config hot-applied without reboot (version " + String(deviceConfig.configVersion) + ")");
        pushEvent(serviceRequest, "ConfigApplied", "version=" + String(deviceConfig.configVersion));
        return true;
    }

    Serial.println("[Service] Config didn't change, do nothing");
    return false;
}
