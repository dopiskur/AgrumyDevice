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

    // API functions
    void apiAuthenticate(DeviceConfig deviceConfig, ServiceRequest serviceRequest, DeviceController& device);
    // Roadmap #67: deviceConfig is a reference so a received config can be hot-applied in place.
    // Returns true when that happened (no reboot) - the caller must then re-copy deviceConfig
    // into the per-module value copies, mirroring setup(). A change to any reboot-required field
    // (transport/identity/sleepDeep, see the .cpp) still reboots and never returns.
    bool apiConfig(DeviceConfig& deviceConfig, ServiceRequest serviceRequest, DeviceController& device);
    ServiceData apiSensorData(DeviceConfig deviceConfig, ServiceRequest serviceRequest);

    // Roadmap #28. service carries whatever apiId/serviceType/servicePoint the caller already had
    // set up - only .endpoint and .header.apiKey are overwritten here. Best-effort: the result is
    // never checked and never retried, see the .cpp for why. commandId (roadmap #34) is included
    // only when >= 0 - present alongside EventType="CommandExecuted", absent for every other event.
    void pushEvent(ServiceRequest service, String eventType, String message, int commandId = -1);

    // Roadmap #34: acks the pending command (best-effort - see the .cpp for why a dropped ack is
    // still safe), performs its action, then reports the outcome via pushEvent above - except
    // Reboot, which has nothing to report from once it fires. No-op if config.pendingCommand is
    // not present. Called from apiConfig() once a fresh config payload has parsed successfully.
    void processPendingCommand(DeviceConfig& config, ServiceRequest serviceRequest, DeviceController& device);

    JsonDocument buildJson();

    // Roadmap #20: masks a secret (apiKey/authKey) for Serial output like SQL data masking - first
    // 4 + last 4 characters visible, rest replaced. Public/static so DeviceController's raw config-
    // JSON debug dump (which embeds apiKey as plaintext) can reuse it instead of duplicating it.
    static String maskSecret(const String &value);

private:
};

// Roadmap #129: the one ServiceController instance, defined in main.cpp - see DeviceModel.h's
// deviceConfig/serviceEndpoint externs for the same reasoning. Consolidating this specifically
// closes the #80 bug class: a separate ServiceController instance with a stale deviceConfig meant
// requestPost()'s cert-pinning check silently used the wrong (empty) servicePublicKey.
extern ServiceController service;

#endif