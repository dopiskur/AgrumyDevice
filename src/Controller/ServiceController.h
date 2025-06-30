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

    // API Functions - now take DeviceController as parameter
    void apiAuthenticate(DeviceConfig deviceConfig, ServiceRequest serviceRequest, DeviceController& device);
    void apiConfig(DeviceConfig deviceConfig, ServiceRequest serviceRequest, DeviceController& device);
    ServiceData apiSensorData(DeviceConfig deviceConfig, ServiceRequest serviceRequest);

    JsonDocument buildJson();

private:
    // ServiceData serviceData;
};
#endif