#ifndef ConfigParser_H
#define ConfigParser_H
#include "Arduino.h"

#include "../Model/DeviceModel.h"

class ConfigParser
{
public:
    // Uses currentConfig as the base so every "|" fallback keeps whatever value is already there when the server omits a key. eventlog error codes: 20 (deserializeJson failure), 21 (missing apiId/apiKey/servicePoint).
    static DeviceConfig parse(const String &configJson, DeviceConfig currentConfig);

    // Masks the "apiKey":"..." field value in place - shared by parse()'s config-sync log and DeviceController::registerDevice()'s own log.
    static String maskApiKeyInJson(const String &json);
};

#endif
