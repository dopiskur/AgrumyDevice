#ifndef OtaController_H
#define OtaController_H
#include "Arduino.h"

class OtaController
{
public:
    // Streams `url` into the inactive OTA partition; returns true once fully written and verified (caller reboots), false leaves the running firmware untouched. expectedSha256: lowercase hex from the catalog manifest, checked while streaming; "" skips the check rather than failing closed.
    static bool update(String url, bool isHttps, const String &servicePublicKey, const String &servicePoint, const String &expectedSha256);
};

#endif
