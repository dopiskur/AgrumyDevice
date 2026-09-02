#ifndef ConfigParser_H
#define ConfigParser_H
#include "Arduino.h"

#include "../Model/DeviceModel.h"

// Roadmap #128: loadConfig() had grown to 188 lines inline in DeviceController.cpp, absorbing
// every new config-sync feature (#107, #34, #12, #39/#115) in the same function - split out
// before the next one (#21 rule engine, #58 PID) adds yet more to it. Same "grounded in the
// actual function, zero behavior change" pattern as #95/#19.
class ConfigParser
{
public:
    // Parses configJson into a DeviceConfig, using currentConfig as the base so every "|" fallback
    // (hysteresis, safety limits, utcOffsetSeconds, etc.) keeps whatever value is already there
    // when the server omits a key - identical contract to the DeviceController::loadConfig() this
    // replaces, including eventlog error codes 20 (deserializeJson failure) and 21 (missing
    // apiId/apiKey/servicePoint, roadmap #107).
    static DeviceConfig parse(const String &configJson, DeviceConfig currentConfig);

    // Roadmap #20: shared between parse()'s config-sync log line and DeviceController::
    // registerDevice()'s own config log - masks the "apiKey":"..." field value in place, single
    // source instead of duplicating the same string surgery in two files.
    static String maskApiKeyInJson(const String &json);
};

#endif
