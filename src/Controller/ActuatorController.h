#ifndef ActuatorController_H
#define ActuatorController_H
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

// Roadmap #110: renamed from ControllerController - "Relay" was too narrow (roadmap #58 PID and
// future output types are not relays), "Actuator" correctly covers any output-driving component
// (relay/PID/valve/motor alike); the old name also read as "controller of a controller".
class ActuatorController
{
public:
    DeviceConfig deviceConfig;

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
    // Shared by the interval and threshold table-driven functions below - the one place that
    // knows relay1..relay8/RELAY_1..RELAY_8 are eight parallel slots, not an array in the model.
    int collectPinsForFunction(RelayFunctionType relayFunction, int pins[8]) const;

    void intervalRelayFunction(RelayFunctionType relayFunction, bool intervalEnabled, int interval, int intervalLength, time_t epochSeconds);
    void thresholdRelayFunction(RelayFunctionType relayFunction, int relayPin, SensorData sensorData);

    // Roadmap #39/#115: localWeekday (0=Sunday..6=Saturday, C's tm_wday) and localSecondsOfDay
    // (0..86399) are computed ONCE per initController() tick from epochSeconds+utcOffsetSeconds
    // and passed to all four calls below, rather than each call re-deriving them - cheap, but no
    // reason to repeat the same gmtime() call four times a tick. slots/slotCount is one relay
    // function's list of windows (RelayLogic::computeAnyScheduleState ORs them together).
    void scheduleRelayFunction(RelayFunctionType relayFunction, const ScheduleWindow slots[], int slotCount,
                                int localWeekday, int localSecondsOfDay);

    // Roadmap #36: the LAST word for a WaterPump-assigned physical relay slot, applied right after
    // thresholdRelayFunction (whatever it just wrote for this pin, in the trailing per-slot loop of
    // initController) - independent of which mode produced that decision. slot (0..7) is the
    // physical relay index, not the discovery order collectPinsForFunction would give - so each
    // slot's on/off history stays correctly independent even if several relays share the WaterPump
    // function. See RelayLogic::runTimeCeilingHit/cooldownActive for the pure math this wraps.
    void applyWaterPumpSafetyLimits(int slot, int pin, time_t epochSeconds);
    void reportSafetyLimitTripped(const String &message);

    time_t waterPumpOnSinceEpoch[8] = {0};
    time_t waterPumpOffSinceEpoch[8] = {0};
    String pendingSafetyEventMessage = "";
};
#endif
