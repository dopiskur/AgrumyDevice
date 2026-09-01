#ifndef ControllerController_H
#define ControllerController_H
#include "Arduino.h"
#include <ArduinoJson.h>

#include "../Model/DeviceModel.h"

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
    Count = 5, // one past WaterPump - sizes the per-function timer state arrays below
};

class ControllerController
{
public:
    DeviceConfig deviceConfig;

    void setupController();
    void initController(SensorData sensorData);

private:
    // Per-relay-function interval-timer state (roadmap #87), indexed by RelayFunctionType - a
    // class member instead of 8 file-level globals, so state no longer secretly lives outside
    // the object that owns it (it worked before only because exactly one ControllerController
    // instance was ever created - see roadmap #77).
    unsigned long millisStart[(int)RelayFunctionType::Count] = {0};
    unsigned long millisStartLenght[(int)RelayFunctionType::Count] = {0};

    // Walks ConfigController.relay1..relay8 and collects the physical pin of every slot assigned
    // to relayFunction into pins[] (caller-provided, must hold 8). Returns how many were found.
    // Shared by the interval and threshold table-driven functions below - the one place that
    // knows relay1..relay8/RELAY_1..RELAY_8 are eight parallel slots, not an array in the model.
    int collectPinsForFunction(RelayFunctionType relayFunction, int pins[8]) const;

    void intervalRelayFunction(RelayFunctionType relayFunction, bool intervalEnabled, int interval, int intervalLenght);
    void thresholdRelayFunction(RelayFunctionType relayFunction, int relayPin, SensorData sensorData);
};
#endif
