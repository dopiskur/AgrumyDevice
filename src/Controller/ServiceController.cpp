#include "Arduino.h"
#include "WiFi.h"
#include "HTTPClient.h"
#include <WiFiClientSecure.h>
#include "NTPClient.h"
#include "ServiceController.h"
#include "DeviceController.h"

#include <ArduinoJson.h>

// Root CA bundle embedded via platformio.ini's board_build.embed_files - see Korak 3 notes there
// for how data/cert/x509_crt_bundle.bin is generated.
extern const uint8_t rootca_crt_bundle_start[] asm("_binary_data_cert_x509_crt_bundle_bin_start");

// Model
static ServiceRequest serviceRequest;
static ServiceEndpoint serviceEndpoint; // zasto ga ne zeli inicijalizirat?!
static String apiAuth;

// Controller
// static DeviceController device; // Commented out due to incomplete type error

ServiceData ServiceController::requestPost(JsonDocument jsonBuffer, ServiceRequest service)
{
    ServiceData serviceData;
    String jsonRequest;

    serializeJsonPretty(jsonBuffer, jsonRequest);

    // wait for WiFi connection
    if ((WiFi.status() == WL_CONNECTED))
    {
        HTTPClient http;
        String serviceURL = service.serviceType + service.servicePoint + service.endpoint;
        Serial.println("[Service] POST: " + serviceURL);
        Serial.println("[Service] apiId: " + service.header.apiId);
        Serial.println("[Service] apiKey: " + service.header.apiKey);
        Serial.println("[Service] authKey: " + apiAuth); // from static

        if (service.isHttps)
        {
            static WiFiClientSecure secureClient;
            if (deviceConfig.servicePublicKey.length() > 0)
            {
                // Self-hosted deployment with a known (often self-signed) certificate: the
                // operator has pinned it via the admin UI into Device.ServicePublicKey, so pin
                // that exact cert here too instead of trusting any public CA for this host.
                secureClient.setCACert(deviceConfig.servicePublicKey.c_str());
            }
            else
            {
                // Default case: servicePoint is expected to carry a publicly-trusted
                // certificate (e.g. Let's Encrypt), so validate against the embedded CA bundle
                // rather than a single hardcoded root - avoids re-flashing every device when
                // that CA rotates or the deployer switches issuers.
                // secureClient is static (reused across calls); clear any CA cert a previous
                // call on this same boot may have set, so a stale pointer can't override the
                // bundle mode we're selecting now (setCACertBundle() doesn't clear it itself).
                secureClient.setCACert(nullptr);
                secureClient.setCACertBundle(rootca_crt_bundle_start);
            }
            http.begin(secureClient, serviceURL);
        }
        else
        {
            // Plain WiFiClient via the implicit http.begin(url) overload - transitional path
            // while http:// service points are still in use.
            http.begin(serviceURL);
        }
        http.addHeader("Content-Type", "application/json");
        http.addHeader("apiId", service.header.apiId);
        http.addHeader("apiKey", service.header.apiKey);
        http.addHeader("Authorization", apiAuth);

        int httpCode = http.POST(jsonRequest);

        // httpCode will be negative on error
        if (httpCode > 0)
        {
            // HTTP header has been send and Server response header has been handled
            Serial.print("[HTTP] Code: ");
            Serial.println(httpCode);
            serviceData.eventlog.errorCode = httpCode;

            // file found at server
            // if (httpCode == HTTP_CODE_OK) {
            if (httpCode == 200 || httpCode == 201)
            {
                serviceData.eventlog.error = false;
                serviceData.payload = http.getString();
            }
            else
            {
                serviceData.eventlog.error = true;
            }

            // write Eventlog
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
    }
    return serviceData;
} // httpRequest() END

// API Requests
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
        //send log;
        device.reset();
    }

    Serial.print("[Heap] before deserializeJson (apiAuthenticate): "); // TEMPORARY DIAGNOSTIC, see Korak 3
    Serial.println(ESP.getFreeHeap());

    DeserializationError error = deserializeJson(payload, serviceData.payload);
    if (error)
    {
        // A truncated/corrupted response body (network drop mid-transfer, or a non-JSON error
        // page) must not silently produce an empty-but-"successful" apiAuth: payload["apiAuth"]
        // on a failed parse just reads back "" anyway, and the caller would carry on believing
        // it has a token. Clearing it explicitly here makes the next request's 401 (if any)
        // reflect the real state and feed Korak 2's failure counter, instead of masking a
        // parsing failure as a successful re-auth.
        Serial.print("[Service] apiAuthenticate: deserializeJson failed: ");
        Serial.println(error.c_str());
        apiAuth = "";
        return;
    }

    String output = payload["apiAuth"];
    apiAuth = output;
    Serial.println("[Service] apiAuthentication authKey: " + apiAuth); // get authentication key
}

void ServiceController::apiConfig(DeviceConfig deviceConfig, ServiceRequest serviceRequest, DeviceController& device)
{
    String configVersion=String(deviceConfig.configVersion); // Casting integer into string for print

    Serial.print("[Service] Current configVersion: ");
    Serial.println(configVersion);

    serviceRequest.endpoint = serviceEndpoint.apiConfig;
    serviceRequest.header.apiId = deviceConfig.apiId;
    serviceRequest.header.apiKey = "";
    serviceRequest.header.apiAuth = apiAuth;

    ServiceData serviceData;
    JsonDocument payload;
    payload["ConfigVersion"] = configVersion; // checking for the version

    serviceData = requestPost(payload, serviceRequest);

    if(serviceData.eventlog.errorCode==401){

        Serial.println("[Service] apiConfig: failed to authenticate: ");      
        apiAuthenticate(deviceConfig,serviceRequest, device);
        serviceRequest.header.apiAuth = apiAuth;
        serviceData = requestPost(payload, serviceRequest); // trying with new token
    }

    // Reboots after MAX_CONSECUTIVE_CONFIG_FAILURES in a row rather than on the first one, so a
    // single transient network hiccup doesn't restart the device - only a run of them, which a
    // reboot can plausibly fix (clears a fragmented heap) but a single retry already couldn't.
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
            device.reboot();
        }
    } else {
        consecutiveFailures = 0;
    }

    // Roadmap #3 (OTA): work out the firmware state from the config we're about to run
    // with - the freshly received one if there is a new config, otherwise the config we
    // booted with (covers the case where the admin sets the flag without bumping
    // configVersion, so the server replies 200 with no body).
    bool   fwFlag    = deviceConfig.firmwareUpdate;
    String fwVersion = deviceConfig.firmwareVersion;
    String fwUrl     = deviceConfig.firmwareUrl;

    if(serviceData.payload!=nullptr){
        Serial.println(serviceData.payload);
        Serial.println("[Service] New config received, saving new config");
        device.saveFile(serviceData.payload, "config.json");
        delay(1000); // Delay for write action

        JsonDocument newCfg;
        if (deserializeJson(newCfg, serviceData.payload) == DeserializationError::Ok) {
            fwFlag    = newCfg["firmwareUpdate"]  | false;
            fwVersion = newCfg["firmwareVersion"] | String("");
            fwUrl     = newCfg["firmwareUrl"]     | String("");
        }
    }

    // Only OTA when the server asks for it AND offers a version different from the one
    // compiled into this image - a stale flag must not loop us into re-download/reboot
    // forever. Do this BEFORE the reboot below so we don't waste a whole config cycle.
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
    }

    if(serviceData.payload!=nullptr){
        device.reboot(); // boot into the newly saved config
    }

     Serial.println("[Service] Config didn't change, do nothing");
     Serial.println(serviceData.payload);


}
