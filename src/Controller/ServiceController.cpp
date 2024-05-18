#include "Arduino.h"
#include "WiFi.h"
#include "HTTPClient.h"
#include "NTPClient.h"
#include "ServiceController.h"

#include <ArduinoJson.h>

// Model
static ServiceRequest serviceRequest;
static ServiceEndpoint serviceEndpoint; // zasto ga ne zeli inicijalizirat?!
static String apiAuth;

// Controller
static DeviceController device;

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

        http.begin(serviceURL);
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
void ServiceController::apiAuthenticate(DeviceConfig deviceConfig, ServiceRequest serviceRequest)
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

    deserializeJson(payload, serviceData.payload);
    // String apiAuth = payload["apiAuth"];
    String output = payload["apiAuth"];
    apiAuth = output;
    Serial.println("[Service] apiAuthentication authKey: " + apiAuth); // get authentication key
}

void ServiceController::apiConfig(DeviceConfig deviceConfig, ServiceRequest serviceRequest)
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
        apiAuthenticate(deviceConfig,serviceRequest);
        serviceRequest.header.apiAuth = apiAuth;
        serviceData = requestPost(payload, serviceRequest); // trying with new token
    }

    if(serviceData.eventlog.error==true){
        // Send log, then reboot
        Serial.print("[Service] Error accesing service point: "); // if still failed, reboot
        Serial.println(serviceData.eventlog.errorCode); // if still failed, reboot
        // device.reboot();
    }

    if(serviceData.payload!=NULL){
        Serial.println(serviceData.payload); 
        Serial.println("[Service] New config received, saving new config");
        device.saveFile(serviceData.payload, "config.json");
        delay(1000); // Delay for write action
        device.reboot();
    }

     Serial.println("[Service] Config didn't change, do nothing"); 
     Serial.println(serviceData.payload);     


}
