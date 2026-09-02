#include "Arduino.h"
#include "LittleFS.h"
#include "FS.h"
#include <ArduinoJson.h>

#include "StorageController.h"

void StorageController::saveFile(String data, String filename)
{
  String path = "/" + filename;
  String tmpPath = path + ".tmp";

  File file = LittleFS.open(tmpPath, "w");
  if (!file)
  {
    Serial.println("Failed to open file for write, formating device");
    LittleFS.format();
    return;
  }

  Serial.print("Saving file: ");
  Serial.println(filename);
  size_t written = file.print(data);
  file.close();

  // Atomic write: only replace the real file once the write to the .tmp copy is confirmed
  // complete. A power loss before this point leaves the old file untouched (the .tmp is
  // orphaned and gets overwritten next call). rename() goes through VFSImpl::rename() -> POSIX
  // rename() -> lfs_rename(), which atomically replaces an existing destination in one
  // filesystem transaction - deliberately NOT preceded by a separate remove(path), which would
  // reopen exactly the half-written-file window this function exists to close.
  if (written != (size_t)data.length())
  {
    Serial.println("[Device] saveFile: incomplete write (" + String((unsigned)written) + "/" + String(data.length()) + " bytes) to " + tmpPath + " - leaving " + path + " untouched");
    LittleFS.remove(tmpPath);
    return;
  }

  if (!LittleFS.rename(tmpPath, path))
  {
    Serial.println("[Device] saveFile: rename " + tmpPath + " -> " + path + " failed");
  }
};

// See StorageController.h for the config-integrity rationale.
void StorageController::saveConfigFile(String newConfigJson)
{
  String currentConfig = loadFile("config.json");
  if (!currentConfig.isEmpty())
  {
    JsonDocument parseCheck;
    if (deserializeJson(parseCheck, currentConfig) == DeserializationError::Ok)
    {
      saveFile(currentConfig, "config.json.bak");
      Serial.println("[Device] Backed up current config.json to config.json.bak");
    }
    else
    {
      Serial.println("[Device] Current config.json failed to parse - not backing it up as a rollback target");
    }
  }

  saveFile(newConfigJson, "config.json");
}

String StorageController::loadFile(String filename)
{
  String path = "/" + filename;
  File file = LittleFS.open(path, "r");

  if (!file || file.isDirectory())
  {
    Serial.println("[Device] loadFile: cannot open " + path);
    return String(); // empty => caller treats the file as absent
  }

  Serial.print("Reading file: ");
  Serial.println(filename);

  // Bounded read - do not trust available() alone: a corrupt LittleFS size
  // field spun this loop forever. config/registration are ~2 KB.
  size_t want = file.size();
  if (want > 16384)
  {
    want = 16384;
  }
  String data;
  data.reserve(want + 1);
  while (data.length() < want)
  {
    int c = file.read();
    if (c < 0)
    {
      break;
    }
    data += (char)c;
  }
  file.close();
  return data;
};

// Roadmap #110: replaces a bare post-write delay() "hope it's committed by now" workaround with
// an actual check - saveFile()'s LittleFS.rename() above already completes synchronously, so this
// bounded poll normally returns on the very first check (0ms lost) and only spends real time if
// that assumption is ever wrong for some platform/flash combination.
bool StorageController::waitForFileCommitted(String filename, unsigned long timeoutMs)
{
  String path = "/" + filename;
  unsigned long start = millis();
  while (!LittleFS.exists(path))
  {
    if (millis() - start >= timeoutMs)
    {
      Serial.println("[Device] waitForFileCommitted: " + path + " still not visible after " + String(timeoutMs) + "ms");
      return false;
    }
    delay(50);
  }
  return true;
}

// Roadmap #110: replaces a bare post-read delay() "hope the race resolved by now" workaround with
// an actual bounded retry - re-reads only when the previous attempt came back empty, instead of
// unconditionally pausing whether or not a retry was ever needed. A file that legitimately doesn't
// exist yet (e.g. first-ever boot, nothing registered) still reads empty on every attempt and
// returns after maxAttempts - this never masks that case, it just stops trusting a single early
// read blindly either way.
String StorageController::loadFileRetry(String filename, int maxAttempts, unsigned long retryDelayMs)
{
  String data = loadFile(filename);
  for (int attempt = 1; data.isEmpty() && attempt < maxAttempts; attempt++)
  {
    delay(retryDelayMs);
    data = loadFile(filename);
  }
  return data;
}

// Roadmap #9. The 70% cap is checked BEFORE every write, which also covers the "a write just
// pushed usage over the line" case the spec calls out: the next 8KB spill re-runs this same
// check and discards, no separate post-write state needed.
bool StorageController::bufferSensorDataToDisk(String payloadJson)
{
  size_t total = LittleFS.totalBytes();
  size_t used = LittleFS.usedBytes();
  if (total == 0 || used * 100 >= total * 70)
  {
    Serial.printf("[Device] Sensor buffer DISCARDED: LittleFS %u/%u bytes (>= 70%% full) - deliberate data loss by design\n", (unsigned)used, (unsigned)total);
    return false;
  }

  // Lazy one-time init per boot: create /buffer and continue numbering after the highest
  // survivor from before the reboot, so chronological order holds across power cycles.
  static int nextIndex = -1;
  if (nextIndex < 0)
  {
    LittleFS.mkdir("/buffer"); // no-op if it already exists
    nextIndex = 1;
    File dir = LittleFS.open("/buffer");
    File entry;
    while (dir && (entry = dir.openNextFile()))
    {
      int n = String(entry.name()).toInt(); // "00042.json" -> 42; non-numeric -> 0, harmless
      entry.close();
      if (n >= nextIndex)
      {
        nextIndex = n + 1;
      }
    }
  }

  char name[24];
  snprintf(name, sizeof(name), "buffer/%05d.json", nextIndex);
  nextIndex++;
  saveFile(payloadJson, name); // #62 atomic tmp+rename helper, reused as-is

  Serial.printf("[Device] Sensor buffer spilled to /%s - LittleFS now %u/%u bytes\n", name, (unsigned)LittleFS.usedBytes(), (unsigned)total);
  return true;
}

String StorageController::oldestBufferedSensorFile()
{
  File dir = LittleFS.open("/buffer");
  if (!dir || !dir.isDirectory())
  {
    return String();
  }

  String best;
  File entry;
  while ((entry = dir.openNextFile()))
  {
    String name = entry.name();
    entry.close();
    if (!name.endsWith(".json")) // skips orphaned .tmp files from an interrupted atomic write
    {
      continue;
    }
    if (best.isEmpty() || name.compareTo(best) < 0)
    {
      best = name;
    }
  }
  return best.isEmpty() ? String() : "buffer/" + best;
}

void StorageController::removeBufferedFile(String filename)
{
  LittleFS.remove("/" + filename);
}
