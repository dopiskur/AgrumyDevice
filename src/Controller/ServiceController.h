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
    void checkConfig(String payload); // For Debug only
    ServiceData requestPost(JsonDocument jsonBuffer, ServiceRequest serviceEndpoint);
    ServiceData requestGet(ServiceRequest service);

    void errorReport(EventLog eventlog);

    void apiAuthenticate(DeviceConfig deviceConfig, ServiceRequest serviceRequest, DeviceController& device);
    // deviceConfig is a reference so a received config can be hot-applied in place; returns true when that happened (no reboot), and the caller must then re-copy deviceConfig into its per-module value copies.
    bool apiConfig(DeviceConfig& deviceConfig, ServiceRequest serviceRequest, DeviceController& device);
    ServiceData apiSensorData(DeviceConfig deviceConfig, ServiceRequest serviceRequest);

    // Best-effort, never checked or retried. commandId is included only when >= 0 (alongside EventType="CommandExecuted").
    void pushEvent(ServiceRequest service, String eventType, String message, int commandId = -1);

    // Acks the pending command, performs its action, then reports the outcome via pushEvent - except Reboot, which never returns. No-op if config.pendingCommand is not present.
    void processPendingCommand(DeviceConfig& config, ServiceRequest serviceRequest, DeviceController& device);

    JsonDocument buildJson();

    // First 4 + last 4 characters visible, rest replaced. Public/static so DeviceController's raw config-JSON debug dump can reuse it.
    static String maskSecret(const String &value);

private:
};

// The one ServiceController instance, defined in main.cpp.
extern ServiceController service;

#endif