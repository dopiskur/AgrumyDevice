#ifndef DeviceController_H
#define DeviceController_H
#include "Arduino.h"
#include "ArduinoJson.h"

#include "../Model/DeviceModel.h"
#include "../Controller/DeviceController.h"
#include "../Controller/SensorController.h"

class DeviceController
{

public:
    DeviceConfig deviceConfig;

    void setupController();

    // Get time
    String getDateTime();

    // Mosfet activation
    void powerRailPrimary(bool state);
    void powerRailSecondary(bool state);

    // Device info
    String macAddr();

    // Device SPIFFS
    void saveFile(String data, String filename);
    String loadFile(String filename);

    void initializeDevice();                        // Setup se WifiAP
    void registerDevice(String configRegistration); // register device on API
    DeviceConfig initializeDefaults(DeviceConfig deviceConfig);
    void initializeWifi();

    String buildConfig(DeviceConfig deviceConfig);
    DeviceConfig loadConfig(String configJson);                                         // load config from json response

    String serviceType(int deviceServiceTypeID); // fetch http, https, mqtt from deviceServiceTypeID

    // Modules
    String rtc();
    String lcd();
    String camera();

    // device actions
    void sleep();
    void firmwareUpdate();
    void reboot();
    void reset();
    void button(); // short press runs query, long press erase device

    String saveValusOnError();

private:
};

#endif
