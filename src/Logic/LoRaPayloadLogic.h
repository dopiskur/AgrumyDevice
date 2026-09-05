#ifndef LoRaPayloadLogic_H
#define LoRaPayloadLogic_H

#include <string>
#include <cmath>

// Pure, native-testable LoRa uplink payload encoding - these bytes go out over the air as the
// LoRaWAN FRMPayload, so field casing per type matches whatever the server's own deserializer
// requires for that entry (api.Relay.ChirpStack.ChirpStackUplinkService/RelayApiController),
// verified against ServiceController.cpp's existing WiFi-profile payloads, not a firmware-side
// style choice: sensor fields are camelCase (matches EfRepository.SensorData's string-key reads),
// config/event/ack fields are PascalCase (matches DeviceConfigPoll/DeviceEventPush/CommandAckRequest's
// case-sensitive JsonElement.Deserialize<T>() on the server).

struct LoRaSensorReading
{
    double temperature = NAN;
    double humidity = NAN;
    double moisture = NAN;
    double battery = NAN;
};

// {"t":"sensor","d":[{...}]} - only non-NAN fields are included to spend as few payload bytes as possible; device/tenant identity is resolved server-side from the DevEUI mapping, never sent over the air.
std::string encodeLoRaSensorUplink(const LoRaSensorReading& reading);

struct LoRaConfigHeartbeat
{
    int configVersion = 0;
    unsigned long uptimeSeconds = 0;
    int rssi = 0;
    unsigned long freeHeapBytes = 0;
    std::string firmwareVersion;
    std::string board;
};

// {"t":"config","ConfigVersion":N,"Uptime":N,"Rssi":N,"FreeHeap":N,"FirmwareVersion":"...","Board":"..."}
std::string encodeLoRaConfigUplink(const LoRaConfigHeartbeat& heartbeat);

// {"t":"event","EventType":"...","Message":"..."}
std::string encodeLoRaEventUplink(const std::string& eventType, const std::string& message);

// {"t":"ack","CommandId":N}
std::string encodeLoRaCommandAckUplink(int commandId);

#endif
