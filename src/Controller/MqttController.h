#ifndef MqttController_H
#define MqttController_H
#include "Arduino.h"
#include <ArduinoJson.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

#include "../Model/DeviceModel.h"

// Forward declarations instead of includes
class DeviceController;

// Additive telemetry channel alongside HTTPS. publishSensorData() connects/publishes/disconnects
// fresh each loop() cycle. The persistent command channel (roadmap #146) is separate and opt-in: a
// mains-powered device that never deep-sleeps can keep one PubSubClient connection open and be
// dispatched a command the instant the server publishes it, instead of waiting for its next HTTP
// poll - see beginPersistentIfEnabled()/poll(). A deep-sleeping device cannot use this at all (the
// connection dies with the sleep), so it always falls back to the normal poll-delivered command path.
class MqttController
{
public:
    // Loads mqttConfig.json once from setup(); leaves publishing disabled if the file is missing/unparseable/blank.
    void begin(DeviceController& device);

    // Best-effort: connects, publishes, disconnects. No-op if never configured or WiFi is down.
    void publishSensorData(DeviceConfig& config, JsonDocument& sensorJson);

    // Opens (or, on a later call, verifies) a persistent connection subscribed to this device's own
    // command topic - no-op unless mqttConfig.json's "persistentCommandChannel" is true. Safe to call
    // every loop() iteration; only reconnects when the connection has actually dropped.
    void beginPersistentIfEnabled(DeviceConfig& config);

    // Pumps the persistent client's receive loop so a subscribed command message is actually
    // delivered to the callback - call this repeatedly during any idle/delay window, never just once.
    void poll();

private:
    String clientId;
    String brokerHost;
    int brokerPort = 1883;
    String username;
    String password;
    bool persistentCommandChannel = false;

    bool publish(const String& topic, const String& payload);
};

// The one MqttController instance, defined in main.cpp.
extern MqttController mqtt;

#endif
