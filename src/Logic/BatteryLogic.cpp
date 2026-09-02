#include "BatteryLogic.h"

double computeDividerBatteryVoltage(double measuredVoltageVolts, double r1Ohms, double r2Ohms)
{
    if (r2Ohms <= 0.0)
    {
        return 0.0;
    }
    return measuredVoltageVolts * (r1Ohms + r2Ohms) / r2Ohms;
}

int computeBatteryPercentFromVoltage(double batteryVoltageVolts)
{
    // Reference points for a single-cell (3.7V nominal) LiPo under light/no load, roughly
    // matching the well-known "3.0V empty / 4.2V full" curve - front-loaded (steep below 3.5V,
    // flat through the 3.7-4.0V midrange, matching how LiPo chemistry actually discharges) so a
    // dead-zone-free linear interpolation across the whole 3.0-4.2V span would otherwise read
    // "half full" long before the pack is actually half discharged.
    static const double voltages[] = {3.00, 3.30, 3.50, 3.60, 3.70, 3.75, 3.80, 3.90, 4.00, 4.10, 4.20};
    static const int percents[] = {0, 5, 10, 20, 40, 50, 60, 70, 80, 90, 100};
    const int points = sizeof(voltages) / sizeof(voltages[0]);

    if (batteryVoltageVolts <= voltages[0])
    {
        return percents[0];
    }
    if (batteryVoltageVolts >= voltages[points - 1])
    {
        return percents[points - 1];
    }

    for (int i = 0; i < points - 1; i++)
    {
        if (batteryVoltageVolts >= voltages[i] && batteryVoltageVolts <= voltages[i + 1])
        {
            double span = voltages[i + 1] - voltages[i];
            double fraction = (batteryVoltageVolts - voltages[i]) / span;
            return percents[i] + (int)((percents[i + 1] - percents[i]) * fraction + 0.5);
        }
    }
    return 0; // unreachable - the two bound checks above cover every value outside the loop's range
}
