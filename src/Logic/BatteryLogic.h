#ifndef BatteryLogic_H
#define BatteryLogic_H

// Roadmap #12: pure, native-testable battery-percentage conversion - pulled out of
// SensorController the same way RelayLogic.h was pulled out of ActuatorController (roadmap
// #19/#95), so the LiPo voltage curve and the divider math can be verified on the host (`pio
// test -e native`) without an ESP32/Arduino toolchain. No Arduino.h, no analogRead/Wire here -
// SensorController.cpp does the actual hardware reads and hands the raw numbers in.

// Roadmap #12: resistor-divider math - V_battery = V_measured * (R1+R2)/R2, where R1/R2 are the
// ACTUAL resistors the user wired (DeviceConfigSensor.BatteryDividerR1/R2), not an abstract
// preset ratio. r2Ohms <= 0 returns 0.0 rather than dividing by it - defensive against an
// unconfigured (0/0) sensor row reaching this before an admin has set real values.
double computeDividerBatteryVoltage(double measuredVoltageVolts, double r1Ohms, double r2Ohms);

// Roadmap #12: approximate 1S LiPo open-circuit-voltage -> state-of-charge curve, piecewise
// linear between known reference points. Deliberately approximate and documented as such (see
// roadmap #12) - a real fuel gauge (MAX17048, coulomb counting) is the recommended path when
// precision matters; this is the NECESSITY/fallback path for VoltageDivider. Clamped to [0,100].
int computeBatteryPercentFromVoltage(double batteryVoltageVolts);

#endif
