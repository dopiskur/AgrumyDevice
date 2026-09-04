#ifndef MqttController_H
#define MqttController_H
#include "Arduino.h"
#include <ArduinoJson.h>

#include "../Model/DeviceModel.h"

// Forward declarations instead of includes
class DeviceController;

// Additive telemetry channel alongside HTTPS - publish-only, connects/publishes/disconnects fresh each loop() cycle instead of staying persistently connected.
class MqttController
{
public:
    // Loads mqttConfig.json once from setup(); leaves publishing disabled if the file is missing/unparseable/blank.
    void begin(DeviceController& device);

    // Best-effort: connects, publishes, disconnects. No-op if never configured or WiFi is down.
    void publishSensorData(DeviceConfig& config, JsonDocument& sensorJson);

private:
    String clientId;
    String brokerHost;
    int brokerPort = 1883;
    String username;
    String password;

    bool publish(const String& topic, const String& payload);
};

// The one MqttController instance, defined in main.cpp.
extern MqttController mqtt;

#endif
