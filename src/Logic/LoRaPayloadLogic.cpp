#include "LoRaPayloadLogic.h"
#include <cmath>
#include <cstdio>

namespace
{
    std::string jsonEscape(const std::string& s)
    {
        std::string out;
        out.reserve(s.size());
        for (char c : s)
        {
            switch (c)
            {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if ((unsigned char)c >= 0x20)
                {
                    out += c;
                }
                break;
            }
        }
        return out;
    }

    std::string formatNumber(double value)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.2f", value);
        return std::string(buf);
    }
}

std::string encodeLoRaSensorUplink(const LoRaSensorReading& reading)
{
    std::string fields;
    auto appendField = [&fields](const char* key, double value)
    {
        if (std::isnan(value))
        {
            return;
        }
        if (!fields.empty())
        {
            fields += ",";
        }
        fields += "\"";
        fields += key;
        fields += "\":";
        fields += formatNumber(value);
    };
    appendField("temperature", reading.temperature);
    appendField("humidity", reading.humidity);
    appendField("moisture", reading.moisture);
    appendField("battery", reading.battery);

    return "{\"t\":\"sensor\",\"d\":[{" + fields + "}]}";
}

std::string encodeLoRaConfigUplink(const LoRaConfigHeartbeat& heartbeat)
{
    char buf[256];
    snprintf(buf, sizeof(buf),
              "{\"t\":\"config\",\"ConfigVersion\":%d,\"Uptime\":%lu,\"Rssi\":%d,\"FreeHeap\":%lu,\"FirmwareVersion\":\"%s\",\"Board\":\"%s\"}",
              heartbeat.configVersion, heartbeat.uptimeSeconds, heartbeat.rssi, heartbeat.freeHeapBytes,
              jsonEscape(heartbeat.firmwareVersion).c_str(), jsonEscape(heartbeat.board).c_str());
    return std::string(buf);
}

std::string encodeLoRaEventUplink(const std::string& eventType, const std::string& message)
{
    return "{\"t\":\"event\",\"EventType\":\"" + jsonEscape(eventType) + "\",\"Message\":\"" + jsonEscape(message) + "\"}";
}

std::string encodeLoRaCommandAckUplink(int commandId)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"t\":\"ack\",\"CommandId\":%d}", commandId);
    return std::string(buf);
}
