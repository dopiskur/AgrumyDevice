#include "DiscoveryLogic.h"
#include <cstdio>

static const std::string AGRUMY_AP_PREFIX = "Agrumy_";

std::string extractAgrumyApMac(const std::string& ssid)
{
    if (ssid.rfind(AGRUMY_AP_PREFIX, 0) != 0)
    {
        return "";
    }
    return ssid.substr(AGRUMY_AP_PREFIX.length());
}

std::string urlEncodeFormValue(const std::string& value)
{
    std::string encoded;
    char hex[4];
    for (unsigned char c : value)
    {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~')
        {
            encoded += (char)c;
        }
        else
        {
            snprintf(hex, sizeof(hex), "%%%02X", c);
            encoded += hex;
        }
    }
    return encoded;
}
