#ifndef DiscoveryLogic_H
#define DiscoveryLogic_H

#include <string>

// Pure, native-testable Agrumy_<mac> AP-name matching - no Arduino.h/WiFi.h here; ServiceController does the actual WiFi.scanNetworks() and hands each SSID in.

// Returns the mac suffix (as macAddr() would report it) when ssid is an Agrumy_ AP name, empty otherwise.
std::string extractAgrumyApMac(const std::string& ssid);

#endif
