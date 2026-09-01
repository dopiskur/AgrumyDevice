#include "Arduino.h"
#include <WiFi.h>
#include <EEPROM.h>
#include "FS.h"
#include "WiFiManager.h"

#include "DeviceController.h"
#include "ServiceController.h"
#include "ControllerController.h"

// No file-local DeviceConfig here: member functions resolve `deviceConfig` to the class
// member, so a file-static copy is dead weight that only ever shadows intent - the config
// arrives via the member, assigned in SensorController before each initController() call.

void ControllerController::setupController(){


}

// Roadmap #87: relay1..relay8/RELAY_1..RELAY_8 are eight parallel struct members, not an array,
// because that is how the model arrives from the API/EF layer - this is the one place that
// flattens them into something loopable, so a ninth relay slot (if the hardware ever grows one)
// is a single line here instead of a new copy-pasted branch in every caller.
int ControllerController::collectPinsForFunction(RelayFunctionType relayFunction, int pins[8]) const
{
    const int configuredType[8] = {
        deviceConfig.configController.relay1, deviceConfig.configController.relay2,
        deviceConfig.configController.relay3, deviceConfig.configController.relay4,
        deviceConfig.configController.relay5, deviceConfig.configController.relay6,
        deviceConfig.configController.relay7, deviceConfig.configController.relay8,
    };
    const int pin[8] = {
        deviceConfig.configPin.RELAY_1, deviceConfig.configPin.RELAY_2,
        deviceConfig.configPin.RELAY_3, deviceConfig.configPin.RELAY_4,
        deviceConfig.configPin.RELAY_5, deviceConfig.configPin.RELAY_6,
        deviceConfig.configPin.RELAY_7, deviceConfig.configPin.RELAY_8,
    };

    int count = 0;
    for (int i = 0; i < 8; i++)
    {
        if (configuredType[i] == (int)relayFunction)
        {
            pins[count++] = pin[i];
        }
    }
    return count;
}

// Roadmap #85: grid-aligned, zero-state redesign - replaces the old millisStart/millisStartLenght
// approach, which tracked "time since last ON/OFF" in RAM and therefore misfired after ANY reboot
// (not just deep sleep - a bare power loss too), since millis() always resets to 0 but the relay
// pin's actual on/off state does not. Instead the on/off state is a pure function of wall-clock
// time: epochSeconds is reduced mod interval to find where "now" falls in the repeating cycle, and
// the relay is on for the first intervalLenght seconds of every cycle - nothing to persist, nothing
// to lose on reboot. Epoch-based (not midnight-based) deliberately: midnight-based would produce
// one short, irregular cycle before every midnight for any interval that doesn't evenly divide 24h
// (e.g. 5h, 7h). A missed cycle across a power outage is accepted as harmless - threshold+hysteresis
// (#10) is the real failsafe, this is scheduling convenience.
// Roadmap #19/#95: the duty-cycle math itself lives in computeIntervalState() (src/Logic/
// RelayLogic.cpp, native-testable) - this wrapper only decides WHETHER to touch these pins at all
// (enabled, interval valid, a relay slot actually assigned) and then does the hardware write.
void ControllerController::intervalRelayFunction(RelayFunctionType relayFunction, bool intervalEnabled, int interval, int intervalLenght, time_t epochSeconds)
{
    if (!intervalEnabled || interval <= 0)
    {
        return;
    }

    int pins[8];
    int pinCount = collectPinsForFunction(relayFunction, pins);
    if (pinCount == 0)
    {
        return; // no relay slot assigned to this function
    }

    bool shouldBeOn = computeIntervalState(interval, intervalLenght, epochSeconds);

    for (int i = 0; i < pinCount; i++)
    {
        pinMode(pins[i], OUTPUT);
        digitalWrite(pins[i], shouldBeOn ? HIGH : LOW);
    }
}

// Dead-zone (hysteresis) relay control (roadmap #10): turns on once the reading crosses the "on"
// side of its threshold, holds on until the reading crosses back past threshold+/-hysteresis, so a
// value sitting right at the threshold cannot chatter the relay every cycle. Ventilation is the
// only function whose "on" direction is inverted (it reacts to humidHigh, not a "Low" threshold,
// because it is exhausting excess humidity rather than replenishing a deficit) - every other
// function turns on BELOW its threshold and off once it climbs back above threshold+hysteresis.
// Roadmap #87: this one function replaces what used to be four near-identical copies
// (relayVentilation/WaterPump/Heating/Light), called once per relay pin from initController below
// (unlike intervalRelayFunction above, each pin is evaluated independently here, even if several
// share the same function - digitalRead(relayPin) keeps each one's on/off state correct on its
// own physical pin).
// Roadmap #19/#95: the dead-zone math itself lives in computeThresholdState() (src/Logic/
// RelayLogic.cpp, native-testable) - this wrapper only resolves WHICH reading/threshold/hysteresis
// apply to relayFunction (a plain lookup, not hardware) and does the pinMode/digitalRead/
// digitalWrite/logging around it.
void ControllerController::thresholdRelayFunction(RelayFunctionType relayFunction, int relayPin, SensorData sensorData)
{
    double reading;
    double threshold;
    double hysteresis;
    bool turnsOnAboveThreshold; // ventilation only

    switch (relayFunction)
    {
    case RelayFunctionType::Ventilation:
        reading = atof(sensorData.humidity.c_str()); // reading, not to be confused with configController.humidHigh (threshold)
        threshold = deviceConfig.configController.humidHigh;
        hysteresis = deviceConfig.configController.humidityHysteresis;
        turnsOnAboveThreshold = true;
        break;
    case RelayFunctionType::Light:
        reading = atof(sensorData.light.c_str()); // reading, not to be confused with configController.lightLow (threshold)
        threshold = deviceConfig.configController.lightLow;
        hysteresis = deviceConfig.configController.lightHysteresis;
        turnsOnAboveThreshold = false;
        break;
    case RelayFunctionType::Heating:
        reading = atof(sensorData.temperature.c_str());
        threshold = deviceConfig.configController.tempLow;
        hysteresis = deviceConfig.configController.temperatureHysteresis;
        turnsOnAboveThreshold = false;
        break;
    case RelayFunctionType::WaterPump:
        reading = atof(sensorData.waterLevel.c_str());
        threshold = deviceConfig.configController.waterLow;
        hysteresis = deviceConfig.configController.waterLevelHysteresis;
        turnsOnAboveThreshold = false;
        break;
    default:
        return; // relay slot unassigned (RelayFunctionType::None) - nothing to control
    }

    pinMode(relayPin, OUTPUT);
    bool isCurrentlyOn = digitalRead(relayPin) == HIGH;
    bool newState = computeThresholdState(isCurrentlyOn, reading, threshold, hysteresis, turnsOnAboveThreshold);

    if (newState != isCurrentlyOn)
    {
        digitalWrite(relayPin, newState ? HIGH : LOW);
        Serial.println(newState ? "[Power rail on]" : "[Power rail off]");
    }
    delay(500);
}

// Roadmap #39: wall-clock schedule control - on for every relay slot assigned to relayFunction
// whenever today's bit is set in daysOfWeek AND localSecondsOfDay falls in [start, start+duration).
// Unlike threshold/interval above, this is a pure function of (config, local time) with no pin
// readback or persisted state at all - the window's on/off state never depends on what it was doing
// a moment ago, so there is nothing to desync after a reboot (also roadmap #85's original goal,
// solved here for schedule mode the same way the epoch-modulo interval algorithm solved it for
// interval mode). v1 does not support a window crossing local midnight - the server rejects
// start+duration > 86400 before it ever reaches a device (DeviceApiController.ScheduleWindowError).
// Roadmap #19/#95: the window-membership math itself lives in computeScheduleState() (src/Logic/
// RelayLogic.cpp, native-testable) - this wrapper only decides WHETHER to touch these pins at all
// (scheduleEnabled) and does the hardware write.
void ControllerController::scheduleRelayFunction(RelayFunctionType relayFunction, bool scheduleEnabled, int daysOfWeek,
                                                   int startSeconds, int durationSeconds, int localWeekday, int localSecondsOfDay)
{
    if (!scheduleEnabled)
    {
        return;
    }

    bool inWindow = computeScheduleState(daysOfWeek, startSeconds, durationSeconds, localWeekday, localSecondsOfDay);

    int pins[8];
    int pinCount = collectPinsForFunction(relayFunction, pins);
    for (int i = 0; i < pinCount; i++)
    {
        pinMode(pins[i], OUTPUT);
        digitalWrite(pins[i], inWindow ? HIGH : LOW);
    }
}

void ControllerController::initController(SensorData sensorData, time_t epochSeconds)
{
    intervalRelayFunction(RelayFunctionType::Ventilation, deviceConfig.configController.ventilationIntervalEnabled,
                           deviceConfig.configController.ventilationInterval, deviceConfig.configController.ventilationIntervalLenght, epochSeconds);
    intervalRelayFunction(RelayFunctionType::Light, deviceConfig.configController.lightIntervalEnabled,
                           deviceConfig.configController.lightInterval, deviceConfig.configController.lightIntervalLenght, epochSeconds);
    intervalRelayFunction(RelayFunctionType::Heating, deviceConfig.configController.heatingIntervalEnabled,
                           deviceConfig.configController.heatingInterval, deviceConfig.configController.heatingIntervalLenght, epochSeconds);
    intervalRelayFunction(RelayFunctionType::WaterPump, deviceConfig.configController.waterPumpIntervalEnabled,
                           deviceConfig.configController.waterPumpInterval, deviceConfig.configController.waterPumpIntervalLenght, epochSeconds);

    // Roadmap #39: gmtime() on a pre-shifted epoch (epochSeconds + utcOffsetSeconds) yields LOCAL
    // wall-clock calendar fields for free - it is pure calendar math with no timezone database
    // involved, so feeding it an already-offset epoch is the standard microcontroller trick for
    // "local time without an IANA database on-device" (see DeviceConfig.utcOffsetSeconds' comment).
    // Computed once here, not once per function call below - four gmtime() calls a tick for the
    // same instant would be pure waste.
    time_t localEpoch = epochSeconds + deviceConfig.utcOffsetSeconds;
    struct tm *localTm = gmtime(&localEpoch);
    int localWeekday = localTm->tm_wday;      // 0=Sunday..6=Saturday
    int localSecondsOfDay = localTm->tm_hour * 3600 + localTm->tm_min * 60 + localTm->tm_sec;

    scheduleRelayFunction(RelayFunctionType::Ventilation, deviceConfig.configController.ventilationScheduleEnabled,
                          deviceConfig.configController.ventilationScheduleDaysOfWeek, deviceConfig.configController.ventilationScheduleStart,
                          deviceConfig.configController.ventilationScheduleDuration, localWeekday, localSecondsOfDay);
    scheduleRelayFunction(RelayFunctionType::Light, deviceConfig.configController.lightScheduleEnabled,
                          deviceConfig.configController.lightScheduleDaysOfWeek, deviceConfig.configController.lightScheduleStart,
                          deviceConfig.configController.lightScheduleDuration, localWeekday, localSecondsOfDay);
    scheduleRelayFunction(RelayFunctionType::Heating, deviceConfig.configController.heatingScheduleEnabled,
                          deviceConfig.configController.heatingScheduleDaysOfWeek, deviceConfig.configController.heatingScheduleStart,
                          deviceConfig.configController.heatingScheduleDuration, localWeekday, localSecondsOfDay);
    scheduleRelayFunction(RelayFunctionType::WaterPump, deviceConfig.configController.waterPumpScheduleEnabled,
                          deviceConfig.configController.waterPumpScheduleDaysOfWeek, deviceConfig.configController.waterPumpScheduleStart,
                          deviceConfig.configController.waterPumpScheduleDuration, localWeekday, localSecondsOfDay);

    // Case numbers must match deviceTypeRelay's IDDeviceTypeRelay seed order (db/agrumyDB-final.sql:
    // 1=Ventilation, 2=Light, 3=Heating, 4=Water pump) - the Web admin dropdown stores that ID
    // directly into relay1..relay8, so a mismatch here silently runs the wrong relay function.
    const int relayType[8] = {
        deviceConfig.configController.relay1, deviceConfig.configController.relay2,
        deviceConfig.configController.relay3, deviceConfig.configController.relay4,
        deviceConfig.configController.relay5, deviceConfig.configController.relay6,
        deviceConfig.configController.relay7, deviceConfig.configController.relay8,
    };
    const int relayPin[8] = {
        deviceConfig.configPin.RELAY_1, deviceConfig.configPin.RELAY_2,
        deviceConfig.configPin.RELAY_3, deviceConfig.configPin.RELAY_4,
        deviceConfig.configPin.RELAY_5, deviceConfig.configPin.RELAY_6,
        deviceConfig.configPin.RELAY_7, deviceConfig.configPin.RELAY_8,
    };

    for (int i = 0; i < 8; i++)
    {
        thresholdRelayFunction((RelayFunctionType)relayType[i], relayPin[i], sensorData);
    }
}
