#include "Arduino.h"
#include <WiFi.h>
#include <EEPROM.h>
#include "FS.h"
#include "WiFiManager.h"

#include "DeviceController.h"
#include "ServiceController.h"
#include "ActuatorController.h"

// Roadmap #129: no deviceConfig member on this class anymore - member functions below resolve
// unqualified `deviceConfig` to the single canonical instance (DeviceModel.h extern), the same one
// every other controller reads.

void ActuatorController::setupController(){


}

// Roadmap #87: relay1..relay8/RELAY_1..RELAY_8 are eight parallel struct members, not an array,
// because that is how the model arrives from the API/EF layer - this is the one place that
// flattens them into something loopable, so a ninth relay slot (if the hardware ever grows one)
// is a single line here instead of a new copy-pasted branch in every caller.
int ActuatorController::collectPinsForFunction(RelayFunctionType relayFunction, int pins[8]) const
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

// Roadmap #21: replaces the pre-#21 intervalRelayFunction/thresholdRelayFunction/
// scheduleRelayFunction trio with one generic evaluator, switching on the rule's own type. Each
// branch calls the SAME pure compute*State() from src/Logic/RelayLogic.cpp (roadmap #19/#95,
// native-testable) that its pre-#21 dispatch function used - only the wrapper/dispatch layer
// changed, not the decision logic itself.
//
// Threshold's reading/direction lookup (roadmap #10) is unchanged from the pre-#21
// thresholdRelayFunction switch: Ventilation reacts to humidity and is the only function whose
// "on" direction is inverted (exhausting excess humidity, not replenishing a deficit); Light/
// Heating/WaterPump all turn on BELOW their threshold and off once the reading climbs back above
// threshold+hysteresis. isCurrentlyOn is the target function's live physical state, needed only
// for this dead-zone math - interval/schedule ignore it entirely, matching roadmap #85's original
// "pure function of wall-clock time" design for interval and the midnight-safe design for schedule.
bool ActuatorController::evaluateRule(const Rule &rule, SensorData sensorData, time_t epochSeconds,
                                       int localWeekday, int localSecondsOfDay, bool isCurrentlyOn) const
{
    switch (rule.type)
    {
    case CONDITION_THRESHOLD:
    {
        double reading;
        bool turnsOnAboveThreshold;
        switch ((RelayFunctionType)rule.targetFunction)
        {
        case RelayFunctionType::Ventilation:
            reading = atof(sensorData.humidity.c_str());
            turnsOnAboveThreshold = true;
            break;
        case RelayFunctionType::Light:
            reading = atof(sensorData.light.c_str());
            turnsOnAboveThreshold = false;
            break;
        case RelayFunctionType::Heating:
            reading = atof(sensorData.temperature.c_str());
            turnsOnAboveThreshold = false;
            break;
        case RelayFunctionType::WaterPump:
            reading = atof(sensorData.waterLevel.c_str());
            turnsOnAboveThreshold = false;
            break;
        default:
            return false; // rule somehow targets no function - never on
        }
        return computeThresholdState(isCurrentlyOn, reading, rule.threshold, rule.hysteresis, turnsOnAboveThreshold);
    }
    case CONDITION_INTERVAL:
        return rule.interval > 0 && computeIntervalState(rule.interval, rule.intervalLength, epochSeconds);
    case CONDITION_SCHEDULE:
        return computeScheduleState(rule.daysOfWeek, rule.start, rule.duration, localWeekday, localSecondsOfDay);
    default:
        return false; // unrecognized type - ConfigParser already skips these at parse time, belt and suspenders
    }
}

// Roadmap #36: state machine combining runTimeCeilingHit()/cooldownActive() into the two
// timestamps this one physical slot needs. Order matters - cooldown is evaluated against the OLD
// offSinceEpoch BEFORE anything below is allowed to touch onSinceEpoch/offSinceEpoch, so an ON
// request that arrives mid-cooldown can never look like a fresh off-transition and reset its own
// clock (which would turn a bounded cooldown into a permanent lockout as long as some mode keeps
// requesting ON every tick).
void ActuatorController::applyWaterPumpSafetyLimits(int slot, int pin, time_t epochSeconds)
{
    // Roadmap #149: RelayIO instead of digitalRead/digitalWrite directly - see RelayIO.h.
    int i2cAddr = deviceConfig.configPin.RELAY_I2C_ADDRESS;
    int i2cSda = deviceConfig.configPin.RELAY_I2C_SDA;
    int i2cScl = deviceConfig.configPin.RELAY_I2C_SCL;
    bool desiredState = relayRead(pin, i2cAddr, i2cSda, i2cScl); // whatever threshold/interval/schedule already wrote this tick
    int maxRunSeconds = deviceConfig.configController.waterPumpMaxRunSeconds;
    int cooldownSeconds = deviceConfig.configController.waterPumpCooldownSeconds;

    bool blockedByCooldown = desiredState && cooldownActive(epochSeconds, waterPumpOffSinceEpoch[slot], cooldownSeconds);

    if (desiredState && !blockedByCooldown && waterPumpOnSinceEpoch[slot] == 0)
    {
        waterPumpOnSinceEpoch[slot] = epochSeconds;
    }

    bool ceilingHit = desiredState && !blockedByCooldown
                       && runTimeCeilingHit(epochSeconds, waterPumpOnSinceEpoch[slot], maxRunSeconds);

    bool finalState = desiredState && !blockedByCooldown && !ceilingHit;

    if (!finalState && waterPumpOnSinceEpoch[slot] != 0)
    {
        // A real on-stretch just ended - whether the underlying mode itself decided off, or a
        // limit above forced it - start the cooldown clock from this moment either way; the
        // physics reason for cooldown (water needs time to drain) applies to every pump-off, not
        // just ones a safety limit caused.
        waterPumpOffSinceEpoch[slot] = epochSeconds;
        waterPumpOnSinceEpoch[slot] = 0;
    }

    if (finalState != desiredState)
    {
        relayWrite(pin, finalState, i2cAddr, i2cSda, i2cScl);
        if (ceilingHit)
        {
            reportSafetyLimitTripped("WaterPump max run time exceeded (" + String(maxRunSeconds) + "s)");
        }
        else if (blockedByCooldown)
        {
            reportSafetyLimitTripped("WaterPump cooldown active, restart blocked");
        }
    }
}

void ActuatorController::reportSafetyLimitTripped(const String &message)
{
    Serial.println("[Safety limit] " + message);
    pendingSafetyEventMessage = message; // last one wins if several slots trip the same tick
}

bool ActuatorController::consumeSafetyLimitEvent(String &outMessage)
{
    if (pendingSafetyEventMessage.length() == 0)
    {
        return false;
    }
    outMessage = pendingSafetyEventMessage;
    pendingSafetyEventMessage = "";
    return true;
}

void ActuatorController::initController(SensorData sensorData, time_t epochSeconds)
{
    // Roadmap #39: gmtime() on a pre-shifted epoch (epochSeconds + utcOffsetSeconds) yields LOCAL
    // wall-clock calendar fields for free - it is pure calendar math with no timezone database
    // involved, so feeding it an already-offset epoch is the standard microcontroller trick for
    // "local time without an IANA database on-device" (see DeviceConfig.utcOffsetSeconds' comment).
    // Computed once here, not once per rule evaluated below.
    time_t localEpoch = epochSeconds + deviceConfig.utcOffsetSeconds;
    struct tm *localTm = gmtime(&localEpoch);
    int localWeekday = localTm->tm_wday;      // 0=Sunday..6=Saturday
    int localSecondsOfDay = localTm->tm_hour * 3600 + localTm->tm_min * 60 + localTm->tm_sec;

    // Roadmap #149: routes through RelayIO so an I2C-expander kit (KC868-A6) works the same as a
    // direct-GPIO one - see RelayIO.h.
    int i2cAddr = deviceConfig.configPin.RELAY_I2C_ADDRESS;
    int i2cSda = deviceConfig.configPin.RELAY_I2C_SDA;
    int i2cScl = deviceConfig.configPin.RELAY_I2C_SCL;

    // Roadmap #21: ONE pass per relay function - was three separate passes before (interval/
    // threshold/schedule), with threshold additionally run per-PIN rather than per-function. Every
    // rule targeting this function is OR'd together (any rule saying "on" wins, user decision:
    // 2026-09-04), then the single result is written to every pin assigned to it.
    const RelayFunctionType functions[4] = {
        RelayFunctionType::Ventilation, RelayFunctionType::Light,
        RelayFunctionType::Heating, RelayFunctionType::WaterPump,
    };
    for (RelayFunctionType function : functions)
    {
        int pins[8];
        int pinCount = collectPinsForFunction(function, pins);
        if (pinCount == 0)
        {
            continue; // no relay slot assigned to this function
        }

        // Threshold rules need the function's CURRENT physical state for their hysteresis math
        // (RelayLogic::computeThresholdState) - read once from the first assigned pin. Every pin
        // sharing one function is kept in sync by the write below, so any one of them is
        // representative (threshold is now evaluated per-function, not per-pin, like interval/
        // schedule always were - roadmap #21 simplification, removes an asymmetry that had no
        // functional reason to exist).
        relayPinMode(pins[0], i2cAddr, i2cSda, i2cScl);
        bool isCurrentlyOn = relayRead(pins[0], i2cAddr, i2cSda, i2cScl);

        bool shouldBeOn = false;
        for (int i = 0; i < deviceConfig.configController.ruleCount; i++)
        {
            const Rule &rule = deviceConfig.configController.rules[i];
            if (rule.targetFunction == (int)function &&
                evaluateRule(rule, sensorData, epochSeconds, localWeekday, localSecondsOfDay, isCurrentlyOn))
            {
                shouldBeOn = true;
            }
        }

        // Roadmap #11: the rain veto is a final AND-NOT gate applied AFTER the OR above, same
        // architectural slot as the WaterPump safety limits below - a Weather condition cannot be a
        // Rule like Threshold/Interval/Schedule, since OR-combining rules means a Weather rule could
        // only ever ADD a reason to turn WaterPump on, never suppress one already decided by another
        // rule (user decision, 2026-09-04).
        if (function == RelayFunctionType::WaterPump && deviceConfig.configController.skipWaterPumpForRain)
        {
            shouldBeOn = false;
        }

        for (int i = 0; i < pinCount; i++)
        {
            relayPinMode(pins[i], i2cAddr, i2cSda, i2cScl);
            relayWrite(pins[i], shouldBeOn, i2cAddr, i2cSda, i2cScl);
        }
    }

    // Roadmap #36: safety limits are the LAST word for WaterPump specifically, applied per
    // PHYSICAL SLOT (not once for the function, unlike the loop above) - each relay slot sharing
    // the WaterPump function keeps its OWN independent on/off-since history
    // (waterPumpOnSinceEpoch/OffSinceEpoch, indexed by physical slot 0..7), matching the pre-#21
    // per-slot application exactly. Case numbers must match deviceTypeRelay's IDDeviceTypeRelay
    // seed order (db/agrumyDB-final.sql: 1=Ventilation, 2=Light, 3=Heating, 4=Water pump) - the Web
    // admin dropdown stores that ID directly into relay1..relay8.
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
        if ((RelayFunctionType)relayType[i] == RelayFunctionType::WaterPump)
        {
            applyWaterPumpSafetyLimits(i, relayPin[i], epochSeconds);
        }
    }
}
