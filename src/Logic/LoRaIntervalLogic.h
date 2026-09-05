#ifndef LoRaIntervalLogic_H
#define LoRaIntervalLogic_H

// Pure, native-testable LoRa uplink pacing - mirrors AgrumyService's api.LoRa.LoRaInterval exactly
// so a LoRa node's own sleep schedule matches what the server assumes it uses; no Arduino/RadioLib
// dependency here, that lives in Controller/LoRaController.

// Seconds between uplinks for spreading factor 7-12, log-linear interpolated between the SF7=30s/SF9=120s/SF12=300s anchors (SF8/10/11 unverified against real hardware, same caveat as the server's LoRaInterval.cs); out-of-range values clamp to the nearest anchor.
double loRaIntervalSecondsForSpreadingFactor(int sf);

// Battery-powered nodes double the base interval to conserve power; mains-powered nodes use it unscaled.
double loRaIntervalSecondsForNode(int sf, bool batteryPowered);

// EU868 ADR datarate index (DR0-DR7) to spreading factor - DR0=SF12 down to DR5=SF7 per RadioLib's EU868 band table; DR6/DR7 are non-SF (250kHz/FSK) and fall back to SF7, the cheapest duty-cycle cost.
int loRaSpreadingFactorForEu868DataRate(int dataRate);

#endif
