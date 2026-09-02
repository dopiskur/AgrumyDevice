#ifndef StorageController_H
#define StorageController_H
#include "Arduino.h"

// Roadmap #127: LittleFS-backed persistence (config.json/deviceRegistration.json plus the roadmap
// #9 store-and-forward sensor buffer) split out of DeviceController.cpp's God Class - this class
// carries no config/network/power concerns, purely file I/O. Grounded on the actual function list,
// zero behavior change, same "#95 pattern" as the rest of the #127 breakup.
class StorageController
{
public:
    // LittleFS lives in the partition labelled "spiffs" (default scheme, no board_build.partitions):
    // esp32dev (4MB, default.csv) = 1408 KB; esp32s3usbotg (8MB, default_8MB.csv)
    // = 1536 KB. It is a separate flash region from the OTA app partitions (ota_0/ota_1), so roadmap
    // #3 OTA never touches it. Stored today: config.json ~2.2 KB + deviceRegistration.json ~0.15 KB
    // => < 2% used, leaving ~1.35-1.5 MB for the roadmap #9 store-and-forward queue.
    //
    // Atomic write: writes to a .tmp copy, only replacing the real file once that write is
    // confirmed complete - a power loss mid-write leaves the old file untouched.
    static void saveFile(String data, String filename);
    static String loadFile(String filename);

    // Roadmap #110: bounded verification helpers - replace the old "delay(1000) and hope" pattern
    // around saveFile()/loadFile() call sites with an actual check, still bounded so neither can
    // hang if the assumption they verify ever turns out to be wrong.
    static bool waitForFileCommitted(String filename, unsigned long timeoutMs = 1000);
    static String loadFileRetry(String filename, int maxAttempts = 5, unsigned long retryDelayMs = 100);

    // Backs up the current config.json to config.json.bak (only if it exists and parses) before
    // atomically replacing it - config integrity, pairs with DeviceController::consumeRollbackTrigger.
    static void saveConfigFile(String newConfigJson);

    // Roadmap #9 store-and-forward (replaces the old saveValusOnError stub). Each file under
    // /buffer/ is one complete, ready-to-POST SensorData JSON array; zero-padded ascending names
    // make lexicographic order == chronological order, so the flush loop never needs a sort.

    // False = payload was DISCARDED because the partition is already >= 70% full (deliberate data
    // loss - the caller reports it, see SensorController's BufferDiscarded event). Uses the same
    // atomic tmp+rename write as config.json (#62), so power loss mid-write never leaves a
    // half-written file for the flush loop to trip on.
    static bool bufferSensorDataToDisk(String payloadJson);

    // Lowest-numbered (oldest) queued file's name relative to loadFile()/removeBufferedFile()
    // (e.g. "buffer/00001.json"), or "" when the queue is empty. Rescans the directory each call -
    // at most ~170 entries even with the partition at its 70% cap, so O(n) per file is fine.
    static String oldestBufferedSensorFile();

    static void removeBufferedFile(String filename);
};

#endif
