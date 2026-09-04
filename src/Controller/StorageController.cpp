#include "Arduino.h"
#include "LittleFS.h"
#include "FS.h"
#include <ArduinoJson.h>

#include "StorageController.h"

// Roadmap #167: return value tells the caller whether data actually reached disk - a rename()
// failure used to only be logged, so a caller like bufferSensorDataToDisk() had no way to know its
// write hadn't really landed before discarding its own in-RAM copy.
bool StorageController::saveFile(String data, String filename)
{
  String path = "/" + filename;
  String tmpPath = path + ".tmp";

  // format() only after repeated failures (real flash degradation), not one transient I/O hiccup - same pattern as MAX_CONSECUTIVE_AUTH_FAILURES.
  static int consecutiveOpenFailures = 0;
  const int MAX_CONSECUTIVE_OPEN_FAILURES = 3;
  File file = LittleFS.open(tmpPath, "w");
  if (!file)
  {
    consecutiveOpenFailures++;
    Serial.printf("[Device] saveFile: failed to open %s for write (%d/%d consecutive)\n",
                   tmpPath.c_str(), consecutiveOpenFailures, MAX_CONSECUTIVE_OPEN_FAILURES);
    if (consecutiveOpenFailures >= MAX_CONSECUTIVE_OPEN_FAILURES)
    {
      Serial.println("[Device] Too many consecutive open failures, formatting filesystem...");
      LittleFS.format();
      consecutiveOpenFailures = 0;
    }
    return false;
  }
  consecutiveOpenFailures = 0;

  Serial.print("Saving file: ");
  Serial.println(filename);
  size_t written = file.print(data);
  file.close();

  // Only replace the real file once the .tmp write is confirmed complete; a power loss before this point leaves the old file untouched. Deliberately NOT preceded by a separate remove(path), which would reopen the half-written-file window this function exists to close.
  if (written != (size_t)data.length())
  {
    Serial.println("[Device] saveFile: incomplete write (" + String((unsigned)written) + "/" + String(data.length()) + " bytes) to " + tmpPath + " - leaving " + path + " untouched");
    LittleFS.remove(tmpPath);
    return false;
  }

  if (!LittleFS.rename(tmpPath, path))
  {
    Serial.println("[Device] saveFile: rename " + tmpPath + " -> " + path + " failed");
    return false;
  }

  return true;
};

// Returns whether the NEW config actually got saved - a failed backup does not block the real
// save (nothing to roll back to yet is not fatal), but a failed primary save is.
bool StorageController::saveConfigFile(String newConfigJson)
{
  String currentConfig = loadFile("config.json");
  if (!currentConfig.isEmpty())
  {
    JsonDocument parseCheck;
    if (deserializeJson(parseCheck, currentConfig) == DeserializationError::Ok)
    {
      if (saveFile(currentConfig, "config.json.bak"))
      {
        Serial.println("[Device] Backed up current config.json to config.json.bak");
      }
      else
      {
        Serial.println("[Device] Failed to back up current config.json - continuing anyway");
      }
    }
    else
    {
      Serial.println("[Device] Current config.json failed to parse - not backing it up as a rollback target");
    }
  }

  return saveFile(newConfigJson, "config.json");
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

  // Roadmap #174: a file over this cap used to be silently truncated and returned as if it were
  // legitimate (short) data - deserializeJson() would then fail on the truncated JSON, which reads
  // to a caller/log as "corrupt config", not "file too large". Refuse outright instead, same
  // explicit-error convention (log + return empty, "caller treats the file as absent" above) as
  // every other failure path in this function.
  const size_t MAX_FILE_SIZE = 16384;
  size_t fileSize = file.size();
  if (fileSize > MAX_FILE_SIZE)
  {
    Serial.printf("[Device] loadFile: %s is %u bytes, exceeds the %u byte cap - refusing to load\n",
                   path.c_str(), (unsigned)fileSize, (unsigned)MAX_FILE_SIZE);
    file.close();
    return String();
  }

  // Reads exactly fileSize bytes, not available()-driven - do not trust available() alone: a
  // corrupt LittleFS size field spun this loop forever before this bound existed.
  String data;
  data.reserve(fileSize + 1);
  while (data.length() < fileSize)
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

// saveFile()'s LittleFS.rename() completes synchronously, so this bounded poll normally returns on the very first check and only spends real time if that assumption is ever wrong.
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

// Re-reads only when the previous attempt came back empty. A file that legitimately doesn't exist yet still reads empty on every attempt and returns after maxAttempts.
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

// The 70% cap is checked BEFORE every write - a write that just pushed usage over the line is caught by the next spill re-running this same check, no separate post-write state needed.
bool StorageController::bufferSensorDataToDisk(String payloadJson)
{
  size_t total = LittleFS.totalBytes();
  size_t used = LittleFS.usedBytes();
  if (total == 0 || used * 100 >= total * 70)
  {
    Serial.printf("[Device] Sensor buffer DISCARDED: LittleFS %u/%u bytes (>= 70%% full) - deliberate data loss by design\n", (unsigned)used, (unsigned)total);
    return false;
  }

  // Lazy one-time init per boot: continue numbering after the highest survivor from before the reboot, so chronological order holds across power cycles.
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
  // Roadmap #167: must not report success (and let the caller drop its own RAM copy) when the
  // data never actually made it to disk - nextIndex is still incremented above even on failure,
  // deliberately, so a later successful spill doesn't reuse this file name.
  if (!saveFile(payloadJson, name))
  {
    Serial.printf("[Device] Sensor buffer spill to /%s FAILED - readings not persisted\n", name);
    return false;
  }

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
    // Roadmap #177: File::name()'s return format (bare "00042.json" vs. already-prefixed
    // "/buffer/00042.json") is not confirmed on this exact ESP32 Arduino core version - if it ever
    // returns the prefixed form, "buffer/" + name below would produce a broken double-prefixed
    // path ("buffer//buffer/00042.json"). Defensive normalization, harmless no-op if name() is
    // already bare.
    if (name.startsWith("/buffer/"))
    {
      name = name.substring(8);
    }
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
