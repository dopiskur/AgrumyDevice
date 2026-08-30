#ifndef ServiceController_H
#define ServiceController_H
#include "Arduino.h"
#include <ArduinoJson.h>

#include "../Model/DeviceModel.h"

// Forward declarations
class DeviceController;
class SensorController;

class ServiceController
{
public:
    DeviceConfig deviceConfig;
    ServiceRequest serviceRequest;

    void checkConfig(String payload); // For Debug only
    ServiceData requestPost(JsonDocument jsonBuffer, ServiceRequest serviceEndpoint);
    ServiceData requestGet(ServiceRequest service);

    void errorReport(EventLog eventlog);

    // API functions
    void apiAuthenticate(DeviceConfig deviceConfig, ServiceRequest serviceRequest, DeviceController& device);
    void apiConfig(DeviceConfig deviceConfig, ServiceRequest serviceRequest, DeviceController& device);
    ServiceData apiSensorData(DeviceConfig deviceConfig, ServiceRequest serviceRequest);

    // Roadmap #28. service carries whatever apiId/serviceType/servicePoint the caller already had
    // set up - only .endpoint and .header.apiKey are overwritten here. Best-effort: the result is
    // never checked and never retried, see the .cpp for why.
    void pushEvent(ServiceRequest service, String eventType, String message);

    JsonDocument buildJson();

private:
};
#endif