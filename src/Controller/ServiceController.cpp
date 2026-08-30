#include "Arduino.h"
#include "WiFi.h"
#include "HTTPClient.h"
#include <WiFiClientSecure.h>
#include "NTPClient.h"
#include "ServiceController.h"
#include "DeviceController.h"

#include <ArduinoJson.h>

// Root CA bundle embedded via platformio.ini board_build.embed_files (see roadmap #3 notes).
extern const uint8_t rootca_crt_bundle_start[] asm("_binary_data_cert_x509_crt_bundle_bin_start");

static ServiceRequest serviceRequest;
static ServiceEndpoint serviceEndpoint;
static String apiAuth;

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
        Serial.println("[Service] apiId: " + service.header.apiId);
        Serial.println("[Service] apiKey: " + service.header.apiKey);
        Serial.println("[Service] authKey: " + apiAuth);

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
    serviceRequest.header.apiAuth = "";

    ServiceData serviceData;
    JsonDocument payload;
    serviceData = requestPost(payload, serviceRequest);

    if(serviceData.eventlog.errorCode==401){
        Serial.println("[Service] Device failed authentication, reseting device to defaults...");
        // Best-effort: this push will very likely also fail (no valid apiAuth yet, that's exactly
        // what just failed) - expected and acceptable, not something to special-case.
        pushEvent(serviceRequest, "AuthFailed", "apiId/apiKey rejected by server");
        device.reset();
    }

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
    Serial.println("[Service] apiAuthentication authKey: " + apiAuth);
}

void ServiceController::apiConfig(DeviceConfig deviceConfig, ServiceRequest serviceRequest, DeviceController& device)
{
    String configVersion=String(deviceConfig.configVersion);

    Serial.print("[Service] Current configVersion: ");
    Serial.println(configVersion);

    serviceRequest.endpoint = serviceEndpoint.apiConfig;
    serviceRequest.header.apiId = deviceConfig.apiId;
    serviceRequest.header.apiKey = "";
    serviceRequest.header.apiAuth = apiAuth;

    ServiceData serviceData;
    JsonDocument payload;
    payload["ConfigVersion"] = configVersion;

    serviceData = requestPost(payload, serviceRequest);

    if(serviceData.eventlog.errorCode==401){

        Serial.println("[Service] apiConfig: failed to authenticate: ");
        apiAuthenticate(deviceConfig,serviceRequest, device);
        serviceRequest.header.apiAuth = apiAuth;
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

    if (receivedNewConfig) {
        Serial.println(serviceData.payload);
        Serial.println("[Service] New config received, saving new config");
        device.saveConfigFile(serviceData.payload); // backs up the old config.json before overwriting it (config integrity)
        delay(1000); // let the write settle

        JsonDocument newCfg;
        if (deserializeJson(newCfg, serviceData.payload) == DeserializationError::Ok) {
            fwFlag    = newCfg["firmwareUpdate"]  | false;
            fwVersion = newCfg["firmwareVersion"] | String("");
            fwUrl     = newCfg["firmwareUrl"]     | String("");
        }
    }

    // OTA only when the server asks AND the offered version differs from this image, so a stale flag can't loop forever; do it before the reboot below.
    extern const char *firmware; // main.cpp
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
        // Config-integrity crash-loop guard: feed the counter ONLY here, right before this
        // specific reboot - never on the OTA reboot above or the too-many-failures reboot,
        // so an unrelated reboot cause never falsely triggers a rollback in setup().
        device.notePendingConfigReboot(millis());
        device.reboot(); // boot into the newly saved config
    } else {
        Serial.println("[Service] Config didn't change, do nothing");
    }
}
