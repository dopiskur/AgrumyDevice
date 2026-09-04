#ifndef DeviceController_H
#define DeviceController_H
#include "Arduino.h"
#include "ArduinoJson.h"

#include "../Model/DeviceModel.h"

// Forward declarations instead of includes
class ServiceController;
class SensorController;

// DeviceController.cpp is a thin facade: file I/O lives in StorageController, power-rail/sleep/reboot/reset in PowerController, OTA download+flash in OtaController, config-field parsing in ConfigParser.
class DeviceController
{

public:
    void setupController();

    String getDateTime();

    // NTP-derived wall-clock seconds, re-synced fresh every boot - ActuatorController's grid-aligned interval formula needs this instead of millis(), which resets on every reboot.
    time_t getEpochSeconds();

    // Mosfet activation
    void powerRailPrimary(bool state);
    void powerRailSecondary(bool state);

    String macAddr();

    // LittleFS-backed. saveFile() writes atomically (temp file + rename), so a power loss
    // mid-write leaves the target file either untouched or fully replaced, never half-written.
    // Returns false if the write/rename did not actually complete (roadmap #167).
    bool saveFile(String data, String filename);
    String loadFile(String filename);

    // Bounded verification (not a fixed delay) around saveFile()/loadFile() call sites.
    bool waitForFileCommitted(String filename, unsigned long timeoutMs = 1000);
    String loadFileRetry(String filename, int maxAttempts = 5, unsigned long retryDelayMs = 100);

    // Backs up the current config.json to config.json.bak (only if it exists and parses) before
    // atomically replacing it - config integrity, pairs with consumeRollbackTrigger() below.
    // Returns whether the new config.json save succeeded.
    bool saveConfigFile(String newConfigJson);

    // Call ONLY from ServiceController::apiConfig()'s "new config received, about to reboot"
    // branch, nowhere else - feeds the crash-loop counter that consumeRollbackTrigger() reads.
    void notePendingConfigReboot(unsigned long uptimeMs);

    // Call once from setup(). True means 3 config-triggered reboots in a row each happened
    // within 60s of their own boot - caller should load config.json.bak instead of config.json.
    bool consumeRollbackTrigger();

    // Set on every config-triggered reboot; call once from setup(). Must be ignored when consumeRollbackTrigger() also returns true - that boot is applying the OLD backup, not the new config this flag was set for.
    bool consumeConfigAppliedPending();

    // Reads and clears any pending core dump the ESP-IDF panic handler wrote on the LAST crash. Call once from setup(). Empty string means no crash since the partition was last cleared.
    String consumeCrashSummary();

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
    // Downloads+flashes a .bin, returns true on success (caller reboots). expectedSha256: lowercase hex from the catalog, "" to skip verification.
    bool firmwareUpdate(String url, bool isHttps, String expectedSha256);
    void reboot();
    void reset();
    void button(); // short press runs query, long press erase device

    // Each file under /buffer/ is one complete, ready-to-POST SensorData JSON array; zero-padded ascending names make lexicographic order == chronological order.
    // False = payload was DISCARDED because the partition is already >= 70% full (deliberate data loss, caller reports it). Atomic tmp+rename write, so power loss mid-write never leaves a half-written file.
    bool bufferSensorDataToDisk(String payloadJson);

    // Lowest-numbered (oldest) queued file's name (e.g. "buffer/00001.json"), or "" when the queue is empty.
    String oldestBufferedSensorFile();

    void removeBufferedFile(String filename);

private:
};

// The one DeviceController instance, defined in main.cpp.
extern DeviceController device;

#endif
