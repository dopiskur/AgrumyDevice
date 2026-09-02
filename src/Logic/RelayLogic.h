#ifndef RelayLogic_H
#define RelayLogic_H

// Roadmap #19/#95: the ON/OFF DECISION for each of the three relay-control modes
// (interval #85, schedule #39, threshold+hysteresis #10), pulled out of ActuatorController
// so it can be unit-tested on the host (PlatformIO's `native` env) without an ESP32/Arduino
// toolchain. Deliberately plain C++ - no Arduino.h, no digitalWrite/pinMode/Serial, nothing
// ESP-IDF-specific - so this header and its .cpp compile identically on a dev laptop and on
// the device. ActuatorController.cpp's intervalRelayFunction/scheduleRelayFunction/
// thresholdRelayFunction are now thin wrappers: gate on "should we touch these pins at all"
// (relayEnabled, an assigned relay slot), call the matching function below for the decision,
// then do the actual pinMode/digitalWrite.
//
// "enabled" flags are deliberately NOT parameters here for interval/schedule: disabled means
// "don't touch these pins at all" (leave them under whatever other mode last set them), which
// is a different outcome than "the pins should be OFF" (an active LOW write) - collapsing that
// distinction into a plain bool return would make a disabled mode indistinguishable from an
// enabled-but-currently-off one. The wrapper checks *Enabled before ever calling these.
#include <ctime>

// ---- Interval (roadmap #85) -------------------------------------------------------------

// Grid-aligned duty cycle: true for the first intervalLength seconds of every `interval`-second
// repeating cycle, keyed off epochSeconds directly (not time-since-last-transition) so the state
// is a pure function of wall-clock time - nothing to persist, nothing to lose on reboot.
// interval <= 0 returns false rather than dividing by it - defensive; ActuatorController's
// caller already guards this case before ever reaching here in production, but a native test can
// still exercise this path directly.
bool computeIntervalState(int interval, int intervalLength, time_t epochSeconds);

// ---- Schedule (roadmap #39) -------------------------------------------------------------

// True whenever today's bit is set in daysOfWeekMask (bit 0 = Sunday .. bit 6 = Saturday,
// matching C's tm_wday directly) AND localSecondsOfDay falls in [startSeconds, startSeconds +
// durationSeconds). "Local" is the caller's responsibility (see DeviceConfig.utcOffsetSeconds) -
// this function only ever sees already-localized weekday/seconds-of-day. v1 does not support a
// window crossing local midnight - the server rejects startSeconds+durationSeconds > 86400
// before it ever reaches a device (DeviceApiController.ScheduleWindowError), so this function
// does not special-case it either.
bool computeScheduleState(int daysOfWeekMask, int startSeconds, int durationSeconds, int localWeekday, int localSecondsOfDay);

// Roadmap #115: one relay function can now have several windows a day (e.g. 6:00-6:30 AND
// 14:00-14:30), replacing the single-window fields above. No dynamic allocation - this runs on
// an embedded target with a fixed-capacity slot array (see MAX_SCHEDULE_SLOTS_PER_FUNCTION), not
// a std::vector.
struct ScheduleWindow
{
    int daysOfWeek = 0;
    int start = 0;
    int duration = 0;
};

// Generous margin over the 3-window example in the roadmap spec (6:00-6:30, 14:00-14:30,
// 20:00-20:15) - trivial RAM cost either way (4 relay functions x 4 slots x 12 bytes = 192 bytes).
const int MAX_SCHEDULE_SLOTS_PER_FUNCTION = 4;

// True if ANY of the first `count` windows in slots[] is currently active (computeScheduleState
// above, OR'd together). count == 0 always returns false - "no windows configured", which the
// caller (ActuatorController::scheduleRelayFunction) treats the same as the old disabled flag:
// leave the pins alone rather than actively writing them off (see that function's comment).
bool computeAnyScheduleState(const ScheduleWindow slots[], int count, int localWeekday, int localSecondsOfDay);

// ---- Threshold + hysteresis (roadmap #10) ------------------------------------------------

// Dead-zone latch: turns on once `reading` crosses the "on" side of `threshold`, stays on until
// it crosses back past threshold +/- hysteresis, and otherwise holds whatever `currentlyOn` was -
// the caller supplies currentlyOn (read from the actual pin state) since this function has no
// state of its own. turnsOnAboveThreshold=true is ventilation's inverted case (reacts to
// humidHigh, exhausting excess humidity rather than replenishing a deficit); every other relay
// function turns on BELOW its threshold.
bool computeThresholdState(bool currentlyOn, double reading, double threshold, double hysteresis, bool turnsOnAboveThreshold);

// ---- Safety limits (roadmap #36) ---------------------------------------------------------
//
// Two independent hard ceilings applied to WaterPump AFTER threshold/interval/schedule (whichever
// wrote the pin last) already decided its state - the actual safety net, working regardless of
// which mode (or a future #58 PID) produced that decision, not an alternative to it. Both are
// plain functions of RAM-only epoch timestamps the caller (ActuatorController) owns and stamps on
// every real on/off transition - see that class's applyWaterPumpSafetyLimits() for the state
// machine. Losing these timestamps on reboot is an accepted, deliberate trade-off: a reboot
// physically de-energizes every relay (GPIO defaults low), so there is no continuous-ON stretch or
// recent-OFF cooldown left to remember - nothing is lost that was ever real (same reasoning as
// roadmap #85's epoch-modulo interval math needing no persisted state either).

// True once a continuous ON stretch (tracked by the caller as onSinceEpoch, 0 = "not currently
// tracked") has run for at least maxRunSeconds - a stuck sensor or a logic error in whichever mode
// requested ON cannot keep the pump running past this regardless. maxRunSeconds <= 0 disables the
// ceiling (never hit) rather than treating 0 as "hit immediately".
bool runTimeCeilingHit(time_t epochSeconds, time_t onSinceEpoch, int maxRunSeconds);

// True while less than cooldownSeconds have passed since the pump's last real OFF transition
// (tracked by the caller as offSinceEpoch, 0 = "never been off since boot" - lets water finish
// draining into the ground before a sensor reading right after a run is trusted again.
// cooldownSeconds <= 0 disables the cooldown (never active).
bool cooldownActive(time_t epochSeconds, time_t offSinceEpoch, int cooldownSeconds);

#endif
