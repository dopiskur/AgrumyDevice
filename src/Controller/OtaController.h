#ifndef OtaController_H
#define OtaController_H
#include "Arduino.h"

class OtaController
{
public:
    // Streams `url` into the inactive OTA partition; returns true once fully written and verified (caller reboots), false leaves the running firmware untouched. Requires isHttps=true and a real 64-char hex expectedSha256 - either missing fails immediately, no soft-skip.
    static bool update(String url, bool isHttps, const String &servicePublicKey, const String &servicePoint, const String &expectedSha256);
};

#endif
