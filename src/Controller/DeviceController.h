#ifndef DeviceController_H
#define DeviceController_H
#include "Arduino.h"
#include "ArduinoJson.h"

#include "../Model/DeviceModel.h"

// Forward declarations instead of includes
class ServiceController;
class SensorController;

// Roadmap #127: this class's own .cpp is now a thin facade - file I/O lives in
// StorageController, power-rail/sleep/reboot/reset in PowerController, OTA download+flash in
// OtaController, config-field parsing in ConfigParser (#128). Every method below keeps its exact
// signature so existing call sites (main.cpp, ServiceController.cpp, SensorController.cpp) needed
// no changes - DeviceController.cpp just delegates instead of implementing each one inline.
class DeviceController
{

public:
    void setupController();

    String getDateTime();

    // Roadmap #85: NTP-derived wall-clock seconds, re-synced fresh every boot (timeClient is a
    // file-local global re-created on every reset - see setupController()) - the grid-aligned
    // interval formula in ActuatorController needs this instead of millis(), which resets on
    // every reboot including a bare power loss and was exactly what made the old interval timers
    // misfire after one.
    time_t getEpochSeconds();

    // Mosfet activation
    void powerRailPrimary(bool state);
    void powerRailSecondary(bool state);

    String macAddr();

    // LittleFS-backed. saveFile() writes atomically (temp file + rename), so a power loss
    // mid-write leaves the target file either untouched or fully replaced, never half-written.
    void saveFile(String data, String filename);
    String loadFile(String filename);

    // Roadmap #110: bounded verification helpers - replace the old "delay(1000) and hope" pattern
    // around saveFile()/loadFile() call sites with an actual check, still bounded so neither can
    // hang if the assumption they verify ever turns out to be wrong.
    bool waitForFileCommitted(String filename, unsigned long timeoutMs = 1000);
    String loadFileRetry(String filename, int maxAttempts = 5, unsigned long retryDelayMs = 100);

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
    // roadmap #3 (OTA): download+flash a .bin, returns true on success (caller reboots).
    // expectedSha256 (roadmap #131): lowercase hex SHA-256 from the catalog, "" to skip verification.
    bool firmwareUpdate(String url, bool isHttps, String expectedSha256);
    void reboot();
    void reset();
    void button(); // short press runs query, long press erase device

    // Roadmap #9 store-and-forward (replaces the old saveValusOnError stub). Each file under
    // /buffer/ is one complete, ready-to-POST SensorData JSON array; zero-padded ascending names
    // make lexicographic order == chronological order, so the flush loop never needs a sort.

    // False = payload was DISCARDED because the partition is already >= 70% full (deliberate data
    // loss - the caller reports it, see SensorController's BufferDiscarded event). Uses the same
    // atomic tmp+rename write as config.json (#62), so power loss mid-write never leaves a
    // half-written file for the flush loop to trip on.
    bool bufferSensorDataToDisk(String payloadJson);

    // Lowest-numbered (oldest) queued file's name relative to loadFile()/removeBufferedFile()
    // (e.g. "buffer/00001.json"), or "" when the queue is empty. Rescans the directory each call -
    // at most ~170 entries even with the partition at its 70% cap, so O(n) per file is fine.
    String oldestBufferedSensorFile();

    void removeBufferedFile(String filename);

private:
};

// Roadmap #129: the one DeviceController instance, defined in main.cpp - see DeviceModel.h's
// deviceConfig/serviceEndpoint externs for the same reasoning.
extern DeviceController device;

#endif
