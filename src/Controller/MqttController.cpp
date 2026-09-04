#include "Arduino.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

#include "MqttController.h"
#include "DeviceController.h"

// Root CA bundle embedded via platformio.ini board_build.embed_files - same bundle ServiceController uses for HTTPS.
extern const uint8_t rootca_crt_bundle_start[] asm("_binary_data_cert_x509_crt_bundle_bin_start");

// PubSubClient's compiled-in default (256 bytes) is too small for a full SensorData JSON payload.
static const uint16_t MQTT_BUFFER_SIZE = 1024;

MqttController mqtt;

void MqttController::begin(DeviceController& device)
{
    clientId = "Agrumy_" + device.macAddr();

    String configJson = device.loadFile("mqttConfig.json");
    if (configJson.isEmpty())
    {
        Serial.println("[Mqtt] No mqttConfig.json - publishing disabled");
        return;
    }

    JsonDocument doc;
    if (deserializeJson(doc, configJson) != DeserializationError::Ok)
    {
        Serial.println("[Mqtt] mqttConfig.json failed to parse - publishing disabled");
        return;
    }

    brokerHost = doc["brokerHost"] | "";
    brokerPort = doc["brokerPort"] | 1883;
    username = doc["username"] | "";
    password = doc["password"] | "";

    if (brokerHost.isEmpty())
    {
        Serial.println("[Mqtt] Broker host blank - publishing disabled");
    }
    else
    {
        Serial.println("[Mqtt] Configured: " + brokerHost + ":" + String(brokerPort));
    }
}

// Fresh TCP+TLS connection per call, no persistent session - matches this firmware's per-cycle connect model.
bool MqttController::publish(const String& topic, const String& payload)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("[Mqtt] WiFi not connected - skipping publish");
        return false;
    }

    bool useTls = (brokerPort == 8883);
    WiFiClientSecure secureClient;
    WiFiClient plainClient;
    if (useTls)
    {
        secureClient.setCACertBundle(rootca_crt_bundle_start);
    }
    PubSubClient client(useTls ? static_cast<Client&>(secureClient) : static_cast<Client&>(plainClient));
    client.setBufferSize(MQTT_BUFFER_SIZE);
    client.setServer(brokerHost.c_str(), (uint16_t)brokerPort);

    bool connected = username.isEmpty()
        ? client.connect(clientId.c_str())
        : client.connect(clientId.c_str(), username.c_str(), password.c_str());

    if (!connected)
    {
        Serial.printf("[Mqtt] Connect to %s:%d failed, state=%d\n", brokerHost.c_str(), brokerPort, client.state());
        return false;
    }

    bool ok = client.publish(topic.c_str(), payload.c_str());
    Serial.println((ok ? "[Mqtt] Published to " : "[Mqtt] Publish failed to ") + topic);
    client.disconnect();
    return ok;
}

void MqttController::publishSensorData(DeviceConfig& config, JsonDocument& sensorJson)
{
    if (brokerHost.isEmpty())
    {
        return;
    }

    String topic = "agrumy/" + String(config.tenantID) + "/" + String(config.deviceID) + "/sensordata";
    String payload;
    serializeJson(sensorJson, payload);
    publish(topic, payload);
}
