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

// Roadmap #110: renamed from ControllerController - "Relay" was too narrow (roadmap #58 PID and
// future output types are not relays), "Actuator" correctly covers any output-driving component
// (relay/PID/valve/motor alike); the old name also read as "controller of a controller".
class ActuatorController
{
public:
    void setupController();

    // epochSeconds: NTP wall-clock time (DeviceController::getEpochSeconds()), needed by the
    // roadmap #85 grid-aligned interval formula below.
    void initController(SensorData sensorData, time_t epochSeconds);

    // Roadmap #36/#28: true (and clears the pending message into outMessage) exactly once per trip
    // - a safety limit forcing the pump off THIS tick, not "still off from a previous trip". The
    // caller (SensorController::pushSensorData, which already owns a correctly-configured
    // ServiceController/ServiceRequest for the #28 event push) polls this once per sensor cycle;
    // repeat trips while the underlying mode keeps requesting ON are left to the server's existing
    // EventDedupeMinutes collapsing, the same way NoInternet already relies on it.
    bool consumeSafetyLimitEvent(String &outMessage);

private:
    // Walks ConfigController.relay1..relay8 and collects the physical pin of every slot assigned
    // to relayFunction into pins[] (caller-provided, must hold 8). Returns how many were found.
    // The one place that knows relay1..relay8/RELAY_1..RELAY_8 are eight parallel slots, not an
    // array in the model.
    int collectPinsForFunction(RelayFunctionType relayFunction, int pins[8]) const;

    // Roadmap #21: replaces the pre-#21 intervalRelayFunction/thresholdRelayFunction/
    // scheduleRelayFunction trio - ONE rule, evaluated to on/off. localWeekday (0=Sunday..6=
    // Saturday, C's tm_wday) and localSecondsOfDay (0..86399) are computed ONCE per
    // initController() tick from epochSeconds+utcOffsetSeconds and passed through rather than
    // re-derived per rule. isCurrentlyOn is the target function's CURRENT physical pin state
    // (needed only by Threshold's hysteresis math - see RelayLogic::computeThresholdState); every
    // rule reads the SAME live state regardless of type, so several Threshold rules for one
    // function each see the real pin, not a stale/differing view.
    bool evaluateRule(const Rule &rule, SensorData sensorData, time_t epochSeconds,
                       int localWeekday, int localSecondsOfDay, bool isCurrentlyOn) const;

    // Roadmap #36: the LAST word for a WaterPump-assigned physical relay slot, applied right after
    // this function's rules have been OR'd and written for this tick - independent of which rule
    // (if any) produced that decision. slot (0..7) is the physical relay index, not the discovery
    // order collectPinsForFunction would give - so each slot's on/off history stays correctly
    // independent even if several relays share the WaterPump function. See RelayLogic::
    // runTimeCeilingHit/cooldownActive for the pure math this wraps.
    void applyWaterPumpSafetyLimits(int slot, int pin, time_t epochSeconds);
    void reportSafetyLimitTripped(const String &message);

    time_t waterPumpOnSinceEpoch[8] = {0};
    time_t waterPumpOffSinceEpoch[8] = {0};
    String pendingSafetyEventMessage = "";
};

// Roadmap #129: the one ActuatorController instance, defined in main.cpp - see DeviceModel.h's
// deviceConfig/serviceEndpoint externs for the same reasoning.
extern ActuatorController controller;

#endif
