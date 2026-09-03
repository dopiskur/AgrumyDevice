#ifndef ActuatorController_H
#define ActuatorController_H
#include "Arduino.h"
#include <ArduinoJson.h>

#include "../Model/DeviceModel.h"
#include "../Logic/RelayLogic.h"
#include "RelayIO.h"

// Forward declarations instead of includes
class DeviceController;
class SensorController;

// Must match deviceTypeRelay's DB seed order (1=Ventilation, 2=Light, 3=Heating, 4=Water pump) - the Web admin dropdown stores this ID directly into ConfigController.relay1..relay8.
enum class RelayFunctionType
{
    None = 0,
    Ventilation = 1,
    Light = 2,
    Heating = 3,
    WaterPump = 4,
};

class ActuatorController
{
public:
    void setupController();

    // epochSeconds: NTP wall-clock time (DeviceController::getEpochSeconds()), needed by the grid-aligned interval formula below.
    void initController(SensorData sensorData, time_t epochSeconds);

    // True (and clears the pending message into outMessage) exactly once per trip - a safety limit forcing the pump off THIS tick, not still off from a previous trip. Caller polls once per sensor cycle.
    bool consumeSafetyLimitEvent(String &outMessage);

private:
    // Walks ConfigController.relay1..relay8 and collects the physical pin of every slot assigned to relayFunction into pins[] (caller-provided, must hold 8). Returns how many were found.
    int collectPinsForFunction(RelayFunctionType relayFunction, int pins[8]) const;

    // localWeekday (0=Sunday..6=Saturday) and localSecondsOfDay (0..86399) are computed ONCE per initController() tick and passed through rather than re-derived per rule. isCurrentlyOn is the target function's CURRENT physical pin state, needed only by Threshold's hysteresis math.
    bool evaluateRule(const Rule &rule, SensorData sensorData, time_t epochSeconds,
                       int localWeekday, int localSecondsOfDay, bool isCurrentlyOn) const;

    // The LAST word for a WaterPump-assigned physical relay slot, applied right after this function's rules are OR'd and written for this tick. slot (0..7) is the physical relay index, not the discovery order collectPinsForFunction gives - so each slot's history stays independent even if several relays share the WaterPump function.
    void applyWaterPumpSafetyLimits(int slot, int pin, time_t epochSeconds);
    void reportSafetyLimitTripped(const String &message);

    time_t waterPumpOnSinceEpoch[8] = {0};
    time_t waterPumpOffSinceEpoch[8] = {0};
    String pendingSafetyEventMessage = "";
};

// The one ActuatorController instance, defined in main.cpp.
extern ActuatorController controller;

#endif
