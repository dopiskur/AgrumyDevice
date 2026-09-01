#ifndef ControllerController_H
#define ControllerController_H
#include "Arduino.h"
#include <ArduinoJson.h>

#include "../Model/DeviceModel.h"
#include "../Logic/RelayLogic.h"

// Forward declarations instead of includes
class DeviceController;
class SensorController;

// Relay-type numbers must match deviceTypeRelay's seed order (db/agrumyDB-final.sql:
// 1=Ventilation, 2=Light, 3=Heating, 4=Water pump) - the Web admin dropdown stores this ID
// directly into ConfigController.relay1..relay8, so these values must never drift from that seed.
enum class RelayFunctionType
{
    None = 0,
    Ventilation = 1,
    Light = 2,
    Heating = 3,
    WaterPump = 4,
};

class ControllerController
{
public:
    DeviceConfig deviceConfig;

    void setupController();

    // epochSeconds: NTP wall-clock time (DeviceController::getEpochSeconds()), needed by the
    // roadmap #85 grid-aligned interval formula below.
    void initController(SensorData sensorData, time_t epochSeconds);

private:
    // Walks ConfigController.relay1..relay8 and collects the physical pin of every slot assigned
    // to relayFunction into pins[] (caller-provided, must hold 8). Returns how many were found.
    // Shared by the interval and threshold table-driven functions below - the one place that
    // knows relay1..relay8/RELAY_1..RELAY_8 are eight parallel slots, not an array in the model.
    int collectPinsForFunction(RelayFunctionType relayFunction, int pins[8]) const;

    void intervalRelayFunction(RelayFunctionType relayFunction, bool intervalEnabled, int interval, int intervalLenght, time_t epochSeconds);
    void thresholdRelayFunction(RelayFunctionType relayFunction, int relayPin, SensorData sensorData);

    // Roadmap #39: localWeekday (0=Sunday..6=Saturday, C's tm_wday) and localSecondsOfDay (0..86399)
    // are computed ONCE per initController() tick from epochSeconds+utcOffsetSeconds and passed to
    // all four calls below, rather than each call re-deriving them - cheap, but no reason to repeat
    // the same gmtime() call four times a tick.
    void scheduleRelayFunction(RelayFunctionType relayFunction, bool scheduleEnabled, int daysOfWeek,
                                int startSeconds, int durationSeconds, int localWeekday, int localSecondsOfDay);
};
#endif
