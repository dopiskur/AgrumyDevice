#include "DiscoveryLogic.h"

static const std::string AGRUMY_AP_PREFIX = "Agrumy_";

std::string extractAgrumyApMac(const std::string& ssid)
{
    if (ssid.rfind(AGRUMY_AP_PREFIX, 0) != 0)
    {
        return "";
    }
    return ssid.substr(AGRUMY_AP_PREFIX.length());
}
