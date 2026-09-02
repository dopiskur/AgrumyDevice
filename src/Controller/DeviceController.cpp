#include "Arduino.h"
#include <WiFi.h>
#include <EEPROM.h>
#include "LittleFS.h"
#include "FS.h"
#include "WiFiManager.h"
#include <HTTPClient.h>       // roadmap #3 (OTA): firmware download
#include <WiFiClientSecure.h> // roadmap #3 (OTA): https firmware download
#include <Update.h>           // roadmap #3 (OTA): Arduino-ESP32 built-in updater

#include "DeviceController.h"
#include "ServiceController.h"

#include <NTPClient.h>
#include <WiFiUdp.h>

// Same CA bundle ServiceController::requestPost() validates against; re-declared here for the OTA download.
extern const uint8_t rootca_crt_bundle_start[] asm("_binary_data_cert_x509_crt_bundle_bin_start");

static ServiceController service; // static resolves a conflict with main

// Roadmap #20: the raw config/registration JSON logged in registerDevice()/loadConfig() below is
// otherwise very useful for debugging sync issues (thresholds, relay assignments, sensor mapping -
// see roadmap #43-47), so it isn't removed wholesale, just its one embedded secret - masks the
// "apiKey":"..." field value in place using ServiceController::maskSecret, leaving the rest as-is.
static String maskApiKeyInJson(const String &json)
{
  const String needle = "\"apiKey\":\"";
  int start = json.indexOf(needle);
  if (start < 0)
  {
    return json;
  }
  start += needle.length();
  int end = json.indexOf('"', start);
  if (end < 0)
  {
    return json;
  }
  return json.substring(0, start) + ServiceController::maskSecret(json.substring(start, end)) + json.substring(end);
}

static DeviceDefaults deviceDefaults;
static DeviceConfig deviceConfig;
static ServiceEndpoint serviceEndpoint;

DeviceRegistration deviceRegistration;
JsonDocument config;

// Roadmap-new (config integrity): survives ESP.restart() (RTC slow memory) but not a real power
// cycle - a hard power loss just loses the streak, which is the safe direction to fail (worst
// case one extra bad-config reboot before rollback kicks in, never a false rollback).
RTC_DATA_ATTR static int rtcRapidConfigRebootCount = 0;

// Roadmap #37: separate from the crash-loop counter above - this is unconditional (set on every
// config-triggered reboot, not just rapid ones) and answers a different question ("did this boot
// follow a just-applied new config") rather than "is this a suspicious streak".
RTC_DATA_ATTR static bool rtcConfigJustAppliedPending = false;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

time_t DeviceController::getEpochSeconds()
{
  return timeClient.getEpochTime();
}

String DeviceController::getDateTime()
{

  time_t epochTime = getEpochSeconds();
  struct tm *ptm = gmtime((time_t *)&epochTime);
  int currentYear = ptm->tm_year + 1900;
  int currentMonth = ptm->tm_mon + 1;
  int monthDay = ptm->tm_mday;
  String currentDate = String(currentYear) + "-" + String(currentMonth) + "-" + String(monthDay) + " " + String(timeClient.getFormattedTime());
  Serial.print("[Device] Current datetime: ");
  Serial.println(currentDate);

  return currentDate;
}

void DeviceController::setupController()
{

  timeClient.begin();
  timeClient.update();
}

// LittleFS lives in the partition labelled "spiffs" (default scheme, no board_build.partitions):
// esp32dev (4MB, default.csv) = 1408 KB; esp32s3usbotg (8MB, default_8MB.csv)
// = 1536 KB. It is a separate flash region from the OTA app partitions (ota_0/ota_1), so roadmap
// #3 OTA never touches it. Stored today: config.json ~2.2 KB + deviceRegistration.json ~0.15 KB
// => < 2% used, leaving ~1.35-1.5 MB for the roadmap #9 store-and-forward queue.
void DeviceController::saveFile(String data, String filename)
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

// See DeviceController.h for the config-integrity rationale.
void DeviceController::saveConfigFile(String newConfigJson)
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

void DeviceController::notePendingConfigReboot(unsigned long uptimeMs)
{
  if (uptimeMs < 60000UL)
  {
    rtcRapidConfigRebootCount++;
    Serial.printf("[Device] Rapid reboot after config update (%lu ms uptime) - streak now %d/3\n", uptimeMs, rtcRapidConfigRebootCount);
  }
  else
  {
    rtcRapidConfigRebootCount = 0; // ran fine for a while first - this config isn't the problem
  }
  rtcConfigJustAppliedPending = true; // roadmap #37 - unconditional, every config-triggered reboot
}

bool DeviceController::consumeRollbackTrigger()
{
  bool trigger = rtcRapidConfigRebootCount >= 3;
  if (trigger)
  {
    rtcRapidConfigRebootCount = 0; // fresh streak after acting on it
  }
  return trigger;
}

bool DeviceController::consumeConfigAppliedPending()
{
  bool pending = rtcConfigJustAppliedPending;
  rtcConfigJustAppliedPending = false;
  return pending;
}

String DeviceController::loadFile(String filename)
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
bool DeviceController::waitForFileCommitted(String filename, unsigned long timeoutMs)
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
String DeviceController::loadFileRetry(String filename, int maxAttempts, unsigned long retryDelayMs)
{
  String data = loadFile(filename);
  for (int attempt = 1; data.isEmpty() && attempt < maxAttempts; attempt++)
  {
    delay(retryDelayMs);
    data = loadFile(filename);
  }
  return data;
}

void DeviceController::initializeWifi()
{

  WiFi.mode(WIFI_STA);
  WiFiManager wifiManager;
  wifiManager.autoConnect();
}

void DeviceController::initializeDevice()
{
  Serial.println("[Device]: initializeDevice");
  WiFi.mode(WIFI_STA);

  WiFi.mode(WIFI_STA);
  WiFiManager wifiManager;
  wifiManager.setCaptivePortalEnable(false);

  WiFiManagerParameter userLogin("login", "User Login", deviceRegistration.userLogin, 128); // id_user can be sent trough wifisave GET
  WiFiManagerParameter userPin("devicePin", "User PIN", deviceRegistration.devicePin, 8);
  WiFiManagerParameter servicePoint("servicePoint", "Service Point (default:api.agrumy.com)", deviceRegistration.servicePoint, 256);

  wifiManager.addParameter(&userLogin);
  wifiManager.addParameter(&userPin);
  wifiManager.addParameter(&servicePoint);

  wifiManager.startConfigPortal(("Agrumy_" + macAddr()).c_str());

  strncpy(deviceRegistration.userLogin, userLogin.getValue(), 128);
  strncpy(deviceRegistration.devicePin, userPin.getValue(), 8);
  strncpy(deviceRegistration.servicePoint, servicePoint.getValue(), 256);

  deviceRegistration.initialize = true;


  JsonDocument config;
  config["userLogin"] = deviceRegistration.userLogin;
  config["devicePin"] = deviceRegistration.devicePin;
  config["servicePoint"] = deviceRegistration.servicePoint;
  if (config["servicePoint"] == "")
  {

    config["servicePoint"] = deviceDefaults.servicePoint;
  }

  String data;
  serializeJsonPretty(config, data);
  Serial.println("[Device] Saving registration data " + data);
  saveFile(data, "deviceRegistration.json");
  waitForFileCommitted("deviceRegistration.json");
  reboot();
};

void DeviceController::registerDevice(String configRegistration)
{
  Serial.println("[Device] Loading registration data " + configRegistration);

  DeserializationError error = deserializeJson(config, configRegistration);

  if (error)
  {
    Serial.print("[Device] RegisterDevice; deserializeJson() failed, reseting to defaults ");
    reset();
  }

  JsonDocument payload;
  payload["macAddress"] = macAddr();
  payload["email"] = config["userLogin"];
  payload["devicePin"] = config["devicePin"];
  payload["serviceType"] = deviceDefaults.serviceType;

  String servicePoint = config["servicePoint"];
  ServiceRequest serviceRequest;
  // Bootstrap call carrying email+PIN before any server config exists, so force HTTPS (roadmap #25).
  serviceRequest.serviceType = serviceType(1, serviceRequest.isHttps);
  serviceRequest.servicePoint = servicePoint;
  serviceRequest.endpoint = serviceEndpoint.apiRegister;

  String serviceURL = serviceRequest.url();
  Serial.println("[Device] Fetching config from " + serviceURL);
  String exitData;
  serializeJsonPretty(payload, exitData);
  Serial.println("[Device] Payload: " + exitData);
  ServiceData serviceData = service.requestPost(payload, serviceRequest);

  if (serviceData.eventlog.errorCode == 401)
  {
    Serial.println("[Device] Wrong user or pin - wiping and restarting the config portal");
    reset();
  }

  // Only a 2xx with a body is a real config. Anything else (5xx, timeout, no WiFi,
  // empty body) must NOT be written to config.json - that would persist an empty
  // file and boot-loop. Back off and reboot to retry a clean registration.
  if ((serviceData.eventlog.errorCode != 200 && serviceData.eventlog.errorCode != 201) ||
      serviceData.payload.isEmpty())
  {
    Serial.println("[Device] Registration failed (code " + String(serviceData.eventlog.errorCode) +
                   "), retrying after reboot");
    delay(10000);
    reboot();
  }

  Serial.println("[Device] config: " + maskApiKeyInJson(serviceData.payload));

  // Roadmap #103: apiConfig()'s #67 parse-gate has no counterpart here - a truncated-but-non-empty
  // body (network hiccup mid-stream) passes both checks above (200/201 status, non-empty) and would
  // be persisted as-is. saveFile()'s atomic write (#62) only protects DISK integrity, not CONTENT
  // validity - a malformed config.json boot-loops on the very next startup. Parse-gate before
  // persisting, same principle as #67: on failure, retry via the same reboot-and-backoff path the
  // HTTP-status/empty-body failure above already uses.
  JsonDocument parseCheck;
  if (deserializeJson(parseCheck, serviceData.payload) != DeserializationError::Ok)
  {
    Serial.println("[Device] Registration payload failed to parse - retrying after reboot");
    delay(10000);
    reboot();
  }

  saveFile(serviceData.payload, "config.json");
  waitForFileCommitted("config.json");
  reboot();
}

String DeviceController::macAddr()
{
  byte mac[6];
  WiFi.macAddress(mac);
  char macAddr[18];
  sprintf(macAddr, "%2X%2X%2X%2X%2X%2X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  for (int i = 0; i < 18; i++) // pad spaces with '0'
  {
    if (macAddr[i] == ' ')
      macAddr[i] = '0';
  }
  return macAddr;
}

void DeviceController::powerRailPrimary(bool state)
{
  const int powerPin = deviceConfig.configPin.POWER_RAIL_PRIMARY;
  pinMode(powerPin, OUTPUT);

  if (state)
  {
    digitalWrite(powerPin, HIGH);
    Serial.println("[Power rail on]");
  }
  else
  {
    digitalWrite(powerPin, LOW);
    Serial.println("[Power rail off]");
  }
  delay(500);
}

void DeviceController::powerRailSecondary(bool state)
{
  const int powerPin = deviceConfig.configPin.POWER_RAIL_SECONDARY;
  pinMode(powerPin, OUTPUT);

  delay(1000);

  if (state)
  {
    digitalWrite(powerPin, HIGH);
    Serial.println("[Power analog sensor on]");
  }
  else
  {
    digitalWrite(powerPin, LOW);
    Serial.println("[Power analog sensor off]");
  }
  delay(500);
}

// Roadmap #26: powers the chip down between cycles; the timer wake is a full reset back
// through setup(). Caller (main loop) decides WHEN sleeping is safe - notably never while
// this device drives relays, since deep sleep drops GPIO outputs and wipes the millis-based
// interval state in ActuatorController.
void DeviceController::sleep()
{
  // uint64 math: seconds * 1e6 overflows int32 for anything past ~35 minutes, and
  // esp_sleep_enable_timer_wakeup takes uint64 microseconds anyway.
  const uint64_t uS_TO_S_FACTOR = 1000000ULL;
  int TIME_TO_SLEEP = deviceConfig.sleepSeconds;
  esp_sleep_enable_timer_wakeup((uint64_t)TIME_TO_SLEEP * uS_TO_S_FACTOR);

  if (deviceConfig.sleepDeep)
  {
    Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Seconds");
    Serial.println("Going to sleep now");
    Serial.flush();
    esp_deep_sleep_start(); // never returns
  }
  else
  {
    Serial.println("Deep sleep disabled");
  }
}

void DeviceController::reboot()
{
  Serial.println("[Rebooting...]");
  ESP.restart();
}

// roadmap #3 (OTA): stream `url` into the inactive OTA partition; returns true once fully written and verified (caller reboots), false leaves the running firmware untouched.
bool DeviceController::firmwareUpdate(String url, bool isHttps)
{
  Serial.println("[Firmware] Starting OTA update from: " + url);

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("[Firmware] No WiFi, aborting OTA");
    return false;
  }

  HTTPClient http;
  WiFiClientSecure secureClient;

  if (isHttps)
  {
    // Roadmap #94: the download host decides which trust to use. A Local-repository URL is served
    // by our own API - if the operator pinned a (self-signed) servicePublicKey, only that cert
    // validates it, exactly as requestPost() does. Any other host (GitHub release asset, a Custom
    // repository) validates against the embedded CA bundle - a pinned cert can never match those,
    // which is precisely why the Local mode exists for pinned-cert installs.
    bool ownServer = deviceConfig.servicePublicKey.length() > 0 &&
                     deviceConfig.servicePoint.length() > 0 &&
                     url.indexOf(deviceConfig.servicePoint) >= 0;
    if (ownServer)
    {
      secureClient.setCACert(deviceConfig.servicePublicKey.c_str());
    }
    else
    {
      secureClient.setCACert(nullptr);
      secureClient.setCACertBundle(rootca_crt_bundle_start);
    }
    http.begin(secureClient, url);
  }
  else
  {
    http.begin(url);
  }
  // Roadmap #94: a GitHub release asset URL answers 302 to objects.githubusercontent.com -
  // without this the GET returns the redirect itself (HTTP 302, no body) and OTA "fails".
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK)
  {
    Serial.printf("[Firmware] Download failed, HTTP code: %d\n", httpCode);
    http.end();
    return false;
  }

  int contentLength = http.getSize();
  if (contentLength <= 0)
  {
    Serial.printf("[Firmware] Invalid content length: %d\n", contentLength);
    http.end();
    return false;
  }

  if (!Update.begin(contentLength))
  {
    Serial.printf("[Firmware] Update.begin failed (need %d bytes free in OTA partition): %s\n",
                  contentLength, Update.errorString());
    http.end();
    return false;
  }

  WiFiClient *stream = http.getStreamPtr();
  size_t written = Update.writeStream(*stream);
  if (written != (size_t)contentLength)
  {
    Serial.printf("[Firmware] Wrote %u/%d bytes\n", (unsigned)written, contentLength);
    Update.abort();
    http.end();
    return false;
  }

  if (!Update.end() || !Update.isFinished())
  {
    Serial.printf("[Firmware] Update did not finish: %s\n", Update.errorString());
    http.end();
    return false;
  }

  http.end();
  Serial.println("[Firmware] Update written and verified");
  return true;
}

void DeviceController::reset()
{
  LittleFS.format();
  ESP.restart();
}

// Roadmap #9. The 70% cap is checked BEFORE every write, which also covers the "a write just
// pushed usage over the line" case the spec calls out: the next 8KB spill re-runs this same
// check and discards, no separate post-write state needed.
bool DeviceController::bufferSensorDataToDisk(String payloadJson)
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

String DeviceController::oldestBufferedSensorFile()
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

void DeviceController::removeBufferedFile(String filename)
{
  LittleFS.remove("/" + filename);
}

DeviceConfig DeviceController::loadConfig(String configJson)
{
  Serial.println("[Device] Load config: " + maskApiKeyInJson(configJson));

  DeserializationError error = deserializeJson(config, configJson);

  if (error)
  {
    Serial.print("[Device] Load Config; deserializeJson() failed: ");
    Serial.println(error.c_str());
    deviceConfig.eventlog.error = true;
    deviceConfig.eventlog.errorCode = 20; // 10 is reserved for deserialize fail
    deviceConfig.eventlog.errorData = error.c_str();

    return deviceConfig;
  }

  String servicePoint = config["servicePoint"];
  // "| """ matters here: a bare assignment from a JSON null (the normal case - no self-signed
  // cert pinned) does not reliably yield an empty ArduinoJson String, so
  // ServiceController::requestPost's `servicePublicKey.length() > 0` check was true for a device
  // with no pinned cert - it then fed that non-empty garbage to setCACert() as if it were a PEM
  // certificate, and mbedTLS rejected the HTTPS handshake with "X509 ... format is invalid".
  String servicePublicKey = config["servicePublicKey"] | "";
  String apiId = config["apiId"];
  String apiKey = config["apiKey"];

  // Roadmap #107: "{}" (or any payload a contract-drifted server field rename produces) is valid,
  // non-empty JSON, so it passes both of apiConfig()'s existing gates (isEmpty/deserializeJson) -
  // a missing key here just silently reads back "". Without this check the device's own identity
  // gets overwritten with "", the next Authenticate 401s, and #97's factory reset fires on what
  // was actually a server-side contract bug. Reject BEFORE any deviceConfig field below is
  // touched, same "keep current, signal the failure" contract as the deserializeJson gate above.
  if (apiId.isEmpty() || apiKey.isEmpty() || servicePoint.isEmpty())
  {
    Serial.println("[Device] Load Config: missing required apiId/apiKey/servicePoint - rejecting (contract drift or malformed payload), keeping current config");
    deviceConfig.eventlog.error = true;
    deviceConfig.eventlog.errorCode = 21; // 20 is deserializeJson failure, 10 is reserved for registerDevice's own gate
    deviceConfig.eventlog.errorData = "missing apiId/apiKey/servicePoint";
    return deviceConfig;
  }
  // deviceConfig is a live member re-parsed in place on every call (not rebuilt from scratch), so
  // a failure flagged above must not linger into the next call that actually succeeds.
  deviceConfig.eventlog.error = false;

  deviceConfig.configVersion = config["configVersion"];

  deviceConfig.tenantID = config["tenantID"];
  deviceConfig.deviceID = config["deviceID"];
  deviceConfig.deviceUnitID = config["deviceUnitID"];
  deviceConfig.deviceUnitZoneID = config["deviceUnitZoneID"];
  deviceConfig.deviceTypeServiceID = config["deviceTypeServiceID"]; // 0 http, 1 https, 2 mqtt

  deviceConfig.apiId = apiId;
  deviceConfig.apiKey = apiKey;
  deviceConfig.servicePoint = servicePoint;
  deviceConfig.servicePublicKey = servicePublicKey;

  deviceConfig.sleepSeconds = config["sleepSeconds"];
  deviceConfig.sleepDeep = config["sleepDeep"];
  // "| 0" keeps the current offset if an older server doesn't send this key, same reasoning as the
  // hysteresis "|" fallbacks below - never silently jump to UTC just because the key was missing.
  deviceConfig.utcOffsetSeconds = config["utcOffsetSeconds"] | deviceConfig.utcOffsetSeconds;
  deviceConfig.deviceSensorEnabled = config["deviceSensorEnabled"];
  deviceConfig.deviceControllerEnabled = config["deviceControllerEnabled"];
  deviceConfig.batteryEnabled = config["batteryEnabled"];
  deviceConfig.enabled = config["enabled"];
  deviceConfig.debug = config["debug"];
  deviceConfig.reboot = config["reboot"];
  deviceConfig.reset = config["reset"];
  deviceConfig.firmwareUpdate = config["firmwareUpdate"];
  deviceConfig.firmwareVersion = config["firmwareVersion"] | String(""); // roadmap #3 (OTA)
  deviceConfig.firmwareUrl = config["firmwareUrl"] | String("");

  // Roadmap #34: "|" keeps the current value if an older server doesn't send this key, same
  // fallback convention as utcOffsetSeconds/hysteresis above.
  deviceConfig.commandVersion = config["commandVersion"] | deviceConfig.commandVersion;
  JsonVariant pendingCommandJson = config["pendingCommand"];
  if (pendingCommandJson.isNull())
  {
    deviceConfig.pendingCommand.present = false;
  }
  else
  {
    deviceConfig.pendingCommand.present = true;
    deviceConfig.pendingCommand.idDeviceCommand = pendingCommandJson["idDeviceCommand"];
    deviceConfig.pendingCommand.actionType = pendingCommandJson["actionType"];
    deviceConfig.pendingCommand.expiresAt = pendingCommandJson["expiresAt"] | String("");
  }

  if (deviceConfig.deviceSensorEnabled)
  {
    JsonObject deviceConfigSensor = config["deviceConfigSensor"];

    deviceConfig.configSensor.sensorBattery = deviceConfigSensor["sensorBattery"];
    // Roadmap #12: same "fall back to whatever value is already here" rule as the hysteresis
    // fields below - an older server that doesn't send these keys yet must not zero out a
    // previously-configured divider calibration.
    deviceConfig.configSensor.batteryDividerR1 = deviceConfigSensor["batteryDividerR1"] | deviceConfig.configSensor.batteryDividerR1;
    deviceConfig.configSensor.batteryDividerR2 = deviceConfigSensor["batteryDividerR2"] | deviceConfig.configSensor.batteryDividerR2;
    deviceConfig.configSensor.sensorTemp = deviceConfigSensor["sensorTemp"];
    deviceConfig.configSensor.sensorTempSoil = deviceConfigSensor["sensorTempSoil"];
    deviceConfig.configSensor.sensorHumid = deviceConfigSensor["sensorHumid"];
    deviceConfig.configSensor.sensorMoist = deviceConfigSensor["sensorMoist"];
    deviceConfig.configSensor.sensorLight = deviceConfigSensor["sensorLight"];
    deviceConfig.configSensor.sensorCo2 = deviceConfigSensor["sensorCo2"];
    deviceConfig.configSensor.sensorTvoc = deviceConfigSensor["sensorTvoc"];
    deviceConfig.configSensor.sensorBarometer = deviceConfigSensor["sensorBarometer"];
    deviceConfig.configSensor.sensorPH = deviceConfigSensor["sensorPH"];
    deviceConfig.configSensor.sensorRainLevel = deviceConfigSensor["sensorRainLevel"];
    deviceConfig.configSensor.sensorWaterLevel = deviceConfigSensor["sensorWaterLevel"];
    deviceConfig.configSensor.sensorWind = deviceConfigSensor["sensorWind"];
  }

  if (deviceConfig.deviceControllerEnabled)
  {
    JsonObject deviceConfigController = config["deviceConfigController"];

    deviceConfig.configController.tempLow = deviceConfigController["tempLow"];
    deviceConfig.configController.tempHigh = deviceConfigController["tempHigh"];
    deviceConfig.configController.humidLow = deviceConfigController["humidLow"];
    deviceConfig.configController.humidHigh = deviceConfigController["humidHigh"];
    deviceConfig.configController.moistLow = deviceConfigController["moistLow"];
    deviceConfig.configController.moistHigh = deviceConfigController["moistHigh"];
    deviceConfig.configController.lightLow = deviceConfigController["lightLow"];
    deviceConfig.configController.lightHigh = deviceConfigController["lightHigh"];
    deviceConfig.configController.waterLow = deviceConfigController["waterLow"];
    deviceConfig.configController.waterHigh = deviceConfigController["waterHigh"];

    // "|" fallback keeps the current value if the server doesn't send these keys (older API),
    // instead of clobbering it with 0.
    deviceConfig.configController.waterLevelHysteresis = deviceConfigController["waterLevelHysteresis"] | deviceConfig.configController.waterLevelHysteresis;
    deviceConfig.configController.temperatureHysteresis = deviceConfigController["temperatureHysteresis"] | deviceConfig.configController.temperatureHysteresis;
    deviceConfig.configController.humidityHysteresis = deviceConfigController["humidityHysteresis"] | deviceConfig.configController.humidityHysteresis;
    deviceConfig.configController.lightHysteresis = deviceConfigController["lightHysteresis"] | deviceConfig.configController.lightHysteresis;

    deviceConfig.configController.ventilationIntervalEnabled = deviceConfigController["ventilationIntervalEnabled"];
    deviceConfig.configController.ventilationInterval = deviceConfigController["ventilationInterval"];
    deviceConfig.configController.ventilationIntervalLength = deviceConfigController["ventilationIntervalLength"];
    deviceConfig.configController.lightIntervalEnabled = deviceConfigController["lightIntervalEnabled"];
    deviceConfig.configController.lightInterval = deviceConfigController["lightInterval"];
    deviceConfig.configController.lightIntervalLength = deviceConfigController["lightIntervalLength"];
    deviceConfig.configController.heatingIntervalEnabled = deviceConfigController["heatingIntervalEnabled"];
    deviceConfig.configController.heatingInterval = deviceConfigController["heatingInterval"];
    deviceConfig.configController.heatingIntervalLength = deviceConfigController["heatingIntervalLength"];
    deviceConfig.configController.waterPumpIntervalEnabled = deviceConfigController["waterPumpIntervalEnabled"];
    deviceConfig.configController.waterPumpInterval = deviceConfigController["waterPumpInterval"];
    deviceConfig.configController.waterPumpIntervalLength = deviceConfigController["waterPumpIntervalLength"];

    // Roadmap #39/#115: a JSON array of windows per relay function, capped at
    // MAX_SCHEDULE_SLOTS_PER_FUNCTION - ArduinoJson's static-buffer parsing model has no dynamic
    // growth on-device, so anything beyond the cap is silently dropped rather than overflowing the
    // fixed array (the server already enforces the same cap when saving, DeviceApiController - this
    // only matters for a pre-cap-enforcement server build or a hand-crafted payload).
    auto parseSchedule = [](JsonArray arr, ScheduleWindow slots[], int &count)
    {
        count = 0;
        for (JsonObject slot : arr)
        {
            if (count >= MAX_SCHEDULE_SLOTS_PER_FUNCTION)
            {
                break;
            }
            slots[count].daysOfWeek = slot["daysOfWeek"];
            slots[count].start = slot["start"];
            slots[count].duration = slot["duration"];
            count++;
        }
    };
    parseSchedule(deviceConfigController["ventilationSchedule"], deviceConfig.configController.ventilationSchedule, deviceConfig.configController.ventilationScheduleCount);
    parseSchedule(deviceConfigController["lightSchedule"], deviceConfig.configController.lightSchedule, deviceConfig.configController.lightScheduleCount);
    parseSchedule(deviceConfigController["heatingSchedule"], deviceConfig.configController.heatingSchedule, deviceConfig.configController.heatingScheduleCount);
    parseSchedule(deviceConfigController["waterPumpSchedule"], deviceConfig.configController.waterPumpSchedule, deviceConfig.configController.waterPumpScheduleCount);

    deviceConfig.configController.relayEnabled = deviceConfigController["relayEnabled"];
    deviceConfig.configController.relay1 = deviceConfigController["relay1"];
    deviceConfig.configController.relay2 = deviceConfigController["relay2"];
    deviceConfig.configController.relay3 = deviceConfigController["relay3"];
    deviceConfig.configController.relay4 = deviceConfigController["relay4"];
    deviceConfig.configController.relay5 = deviceConfigController["relay5"];
    deviceConfig.configController.relay6 = deviceConfigController["relay6"];
    deviceConfig.configController.relay7 = deviceConfigController["relay7"];
    deviceConfig.configController.relay8 = deviceConfigController["relay8"];
  }

  return deviceConfig;
};

String DeviceController::serviceType(int deviceServiceTypeID, bool& isHttps)
{
  String serviceType;

  switch (deviceServiceTypeID)
  {
  case 0:
    serviceType = "http://";
    isHttps = false;
    break;
  case 1:
    serviceType = "https://";
    isHttps = true;
    break;
  case 2:
    serviceType = "mqtt://";
    isHttps = false;
    break;
  default:
    serviceType = "https://";
    isHttps = true;
    break;
  }

  return serviceType;
}
