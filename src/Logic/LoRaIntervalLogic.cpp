#include "LoRaIntervalLogic.h"
#include <cmath>

namespace
{
    struct Anchor
    {
        int sf;
        double seconds;
    };
    const Anchor anchors[] = {{7, 30.0}, {9, 120.0}, {12, 300.0}};
    const int anchorCount = sizeof(anchors) / sizeof(anchors[0]);
}

double loRaIntervalSecondsForSpreadingFactor(int sf)
{
    int clamped = sf;
    if (clamped < anchors[0].sf)
    {
        clamped = anchors[0].sf;
    }
    if (clamped > anchors[anchorCount - 1].sf)
    {
        clamped = anchors[anchorCount - 1].sf;
    }

    for (int i = 0; i < anchorCount - 1; i++)
    {
        int loSf = anchors[i].sf, hiSf = anchors[i + 1].sf;
        double loSeconds = anchors[i].seconds, hiSeconds = anchors[i + 1].seconds;
        if (clamped < loSf || clamped > hiSf)
        {
            continue;
        }
        if (clamped == loSf)
        {
            return loSeconds;
        }
        if (clamped == hiSf)
        {
            return hiSeconds;
        }

        // Log-linear, not linear - airtime/duty-cycle cost grows roughly exponentially with SF.
        double t = (double)(clamped - loSf) / (hiSf - loSf);
        double logSeconds = std::log(loSeconds) + t * (std::log(hiSeconds) - std::log(loSeconds));
        return std::exp(logSeconds);
    }
    return anchors[anchorCount - 1].seconds;
}

double loRaIntervalSecondsForNode(int sf, bool batteryPowered)
{
    double base = loRaIntervalSecondsForSpreadingFactor(sf);
    return batteryPowered ? base * 2.0 : base;
}

int loRaSpreadingFactorForEu868DataRate(int dataRate)
{
    if (dataRate >= 0 && dataRate <= 5)
    {
        return 12 - dataRate;
    }
    return 7;
}
