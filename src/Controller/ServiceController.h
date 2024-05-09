#ifndef ServiceController_H
#define ServiceController_H
#include "Arduino.h"
#include <ArduinoJson.h>

#include "../Model/DeviceModel.h"
#include "../Controller/DeviceController.h"
#include "../Controller/SensorController.h"

class ServiceController
{
public:
    DeviceConfig deviceConfig;
    ServiceRequest serviceRequest;

    void checkConfig(String payload); // For Debug only
    ServiceData requestPost(JsonDocument jsonBuffer, ServiceRequest serviceEndpoint);
    ServiceData requestGet(ServiceRequest service);

    void errorReport(EventLog eventlog);

    // API Functions
    void apiAuthenticate(DeviceConfig deviceConfig, ServiceRequest serviceRequest);
    void apiConfig(DeviceConfig deviceConfig, ServiceRequest serviceRequest);
    ServiceData apiSensorData(DeviceConfig deviceConfig, ServiceRequest serviceRequest);

    JsonDocument buildJson();

private:
    // ServiceData serviceData;
};
#endif