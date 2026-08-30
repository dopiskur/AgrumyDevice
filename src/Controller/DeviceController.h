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

    // LittleFS-backed. saveFile() writes atomically (temp file + rename), so a power loss
    // mid-write leaves the target file either untouched or fully replaced, never half-written.
    void saveFile(String data, String filename);
    String loadFile(String filename);

    // Backs up the current config.json to config.json.bak (only if it exists and parses) before
    // atomically replacing it - config integrity, pairs with consumeRollbackTrigger() below.
    void saveConfigFile(String newConfigJson);

    // Call ONLY from ServiceController::apiConfig()'s "new config received, about to reboot"
    // branch, nowhere else - feeds the crash-loop counter that consumeRollbackTrigger() reads.
    void notePendingConfigReboot(unsigned long uptimeMs);

    // Call once from setup(). True means 3 config-triggered reboots in a row each happened
    // within 60s of their own boot - caller should load config.json.bak instead of config.json.
    bool consumeRollbackTrigger();

    // Roadmap #37. Set unconditionally by notePendingConfigReboot() on every config-triggered
    // reboot (not just rapid ones) - call once from setup() to know whether THIS boot is the
    // direct result of applying a newly-received config, so the caller can confirm that back to
    // the server. Must be ignored when consumeRollbackTrigger() also returns true - that boot is
    // applying the OLD backup, not the new config this flag was set for.
    bool consumeConfigAppliedPending();

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
