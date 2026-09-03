#ifndef StorageController_H
#define StorageController_H
#include "Arduino.h"

class StorageController
{
public:
    // LittleFS "spiffs" partition: ~1.4-1.5MB depending on board, separate flash region from the OTA app partitions (ota_0/ota_1). config.json + deviceRegistration.json use <2%, leaving the rest for the store-and-forward queue.
    // Atomic write: writes to a .tmp copy, only replacing the real file once that write is confirmed complete - a power loss mid-write leaves the old file untouched.
    static void saveFile(String data, String filename);
    static String loadFile(String filename);

    // Bounded verification (not a fixed delay) around saveFile()/loadFile() call sites.
    static bool waitForFileCommitted(String filename, unsigned long timeoutMs = 1000);
    static String loadFileRetry(String filename, int maxAttempts = 5, unsigned long retryDelayMs = 100);

    // Backs up the current config.json to config.json.bak (only if it exists and parses) before atomically replacing it.
    static void saveConfigFile(String newConfigJson);

    // Each file under /buffer/ is one complete, ready-to-POST SensorData JSON array; zero-padded ascending names make lexicographic order == chronological order.
    // False = payload was DISCARDED because the partition is already >= 70% full (deliberate data loss, caller reports it). Atomic tmp+rename write.
    static bool bufferSensorDataToDisk(String payloadJson);

    // Lowest-numbered (oldest) queued file's name (e.g. "buffer/00001.json"), or "" when the queue is empty.
    static String oldestBufferedSensorFile();

    static void removeBufferedFile(String filename);
};

#endif
