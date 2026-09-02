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
    // vs GitHub/Custom trust decision (roadmap #94) they drive.
    static bool update(String url, bool isHttps, const String &servicePublicKey, const String &servicePoint);
};

#endif
