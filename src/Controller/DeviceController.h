#ifndef DeviceController_H
#define DeviceController_H
#include "Arduino.h"
#include "ArduinoJson.h"

#include "../Model/DeviceModel.h"

// Forward declarations instead of includes
class ServiceController;
class SensorController;

class DeviceController
{

public:
    DeviceConfig deviceConfig;

    void setupController();

    String getDateTime();

    // Mosfet activation
    void powerRailPrimary(bool state);
    void powerRailSecondary(bool state);

    String macAddr();

    // LittleFS-backed
    void saveFile(String data, String filename);
    String loadFile(String filename);

    void initializeDevice(); // sets up the WiFi AP
    void registerDevice(String configRegistration);
    DeviceConfig initializeDefaults(DeviceConfig deviceConfig);
    void initializeWifi();

    String buildConfig(DeviceConfig deviceConfig);
    DeviceConfig loadConfig(String configJson);

    String serviceType(int deviceServiceTypeID, bool& isHttps); // maps deviceServiceTypeID to http/https/mqtt

    String rtc();
    String lcd();
    String camera();

    void sleep();
    bool firmwareUpdate(String url, bool isHttps); // roadmap #3 (OTA): download+flash a .bin, returns true on success (caller reboots)
    void reboot();
    void reset();
    void button(); // short press runs query, long press erase device

    String saveValusOnError();

private:
};

#endif
