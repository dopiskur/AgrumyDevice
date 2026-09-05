#ifndef DiscoveryLogic_H
#define DiscoveryLogic_H

#include <string>

// Pure, native-testable Agrumy_<mac> AP-name matching - no Arduino.h/WiFi.h here; ServiceController does the actual WiFi.scanNetworks() and hands each SSID in.

// Returns the mac suffix (as macAddr() would report it) when ssid is an Agrumy_ AP name, empty otherwise.
std::string extractAgrumyApMac(const std::string& ssid);

// Percent-encodes one application/x-www-form-urlencoded field value - keeps [A-Za-z0-9-_.~], encodes everything else (including space, as %20) as %XX.
std::string urlEncodeFormValue(const std::string& value);

#endif
