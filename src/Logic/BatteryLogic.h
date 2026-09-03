#ifndef BatteryLogic_H
#define BatteryLogic_H

// Pure, native-testable battery-percentage conversion - no Arduino.h/analogRead/Wire here; SensorController.cpp does the actual hardware reads and hands the raw numbers in.

// V_battery = V_measured * (R1+R2)/R2, where R1/R2 are the ACTUAL resistors wired, not an abstract preset ratio. r2Ohms <= 0 returns 0.0 rather than dividing by it.
double computeDividerBatteryVoltage(double measuredVoltageVolts, double r1Ohms, double r2Ohms);

// Approximate 1S LiPo open-circuit-voltage -> state-of-charge curve, piecewise linear between known reference points. A real fuel gauge (coulomb counting) is more precise; this is the fallback path. Clamped to [0,100].
int computeBatteryPercentFromVoltage(double batteryVoltageVolts);

#endif
