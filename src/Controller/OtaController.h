#ifndef OtaController_H
#define OtaController_H
#include "Arduino.h"

// Roadmap #127/#3: OTA download+flash split out of DeviceController.cpp's God Class.
class OtaController
{
public:
    // Streams `url` into the inactive OTA partition; returns true once fully written and verified
    // (caller reboots), false leaves the running firmware untouched. servicePublicKey/servicePoint
    // are passed in rather than read from a shared DeviceConfig - see .cpp for the Local-repository
    // vs GitHub/Custom trust decision (roadmap #94) they drive. expectedSha256 (roadmap #131):
    // lowercase hex SHA-256 of the .bin from the server's catalog manifest, checked against a hash
    // computed while streaming the write - "" (no catalog hash for this source/entry) skips the
    // check rather than failing closed, matching how a missing manifest is already tolerated
    // server-side (FirmwareCatalogService).
    static bool update(String url, bool isHttps, const String &servicePublicKey, const String &servicePoint, const String &expectedSha256);
};

#endif
