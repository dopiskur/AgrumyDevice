#include "Arduino.h"
#include <WiFi.h>
#include <EEPROM.h>
#include "WiFiManager.h"

#include "DeviceController.h"
#include "ServiceController.h"
#include "ConfigParser.h"
#include "StorageController.h"
#include "PowerController.h"
#include "OtaController.h"

#include <NTPClient.h>
#include <WiFiUdp.h>
#include <esp_core_dump.h> // roadmap #135

// Roadmap #129: `service`/`deviceConfig`/`serviceEndpoint` are the single canonical instances
// (declared in main.cpp, extern-visible via ServiceController.h/DeviceModel.h) - this file no
// longer keeps its own separate copies.

static DeviceDefaults deviceDefaults;

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

// Roadmap #127: file I/O moved to StorageController - these stay thin wrappers so every existing
// `device.saveFile(...)`/`device.loadFile(...)`/etc. call site keeps working unchanged.
void DeviceController::saveFile(String data, String filename)
{
  StorageController::saveFile(data, filename);
};

// See StorageController.h for the config-integrity rationale.
void DeviceController::saveConfigFile(String newConfigJson)
{
  StorageController::saveConfigFile(newConfigJson);
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

// Roadmap #135. exc_bt_info.bt holds up to 16 return addresses but the reported Message stays
// short on purpose - symbolicating ANY of them still needs firmware.elf (xtensa-esp32-elf-addr2line
// against the exact version this device reports) regardless of how many are included, so more than
// a handful adds length without adding usable signal without that file already in hand.
static const uint32_t MaxCrashBacktraceAddresses = 8;

String DeviceController::consumeCrashSummary()
{
  if (esp_core_dump_image_check() != ESP_OK)
  {
    return ""; // no dump since the partition was last cleared - the common case, every normal boot
  }

  // Per esp_core_dump.h's own documented usage: heap-allocated, not a ~250+ byte stack frame that
  // would otherwise sit in setup()'s frame for the rest of the function on every single boot.
  esp_core_dump_summary_t *summary = (esp_core_dump_summary_t *)malloc(sizeof(esp_core_dump_summary_t));
  String result = "";
  if (summary != nullptr && esp_core_dump_get_summary(summary) == ESP_OK)
  {
    String backtrace = "";
    uint32_t depth = summary->exc_bt_info.depth;
    if (depth > MaxCrashBacktraceAddresses)
    {
      depth = MaxCrashBacktraceAddresses;
    }
    for (uint32_t i = 0; i < depth; i++)
    {
      if (i > 0)
      {
        backtrace += ",";
      }
      backtrace += "0x" + String(summary->exc_bt_info.bt[i], HEX);
    }

    result = "task=" + String(summary->exc_task) +
             " pc=0x" + String(summary->exc_pc, HEX) +
             " cause=" + String(summary->ex_info.exc_cause) +
             " vaddr=0x" + String(summary->ex_info.exc_vaddr, HEX) +
             (summary->exc_bt_info.corrupted ? " bt(corrupted)=" : " bt=") + backtrace;
    Serial.println("[Device] Pending crash dump found: " + result);
  }
  else
  {
    Serial.println("[Device] Core dump present but its summary could not be read");
  }
  free(summary);

  // Best-effort, same as the rest of #28's reporting (pushEvent never retries): erase regardless of
  // whether the summary above actually got formatted, so a corrupt/unreadable dump never wedges
  // every future boot into re-attempting the same failed read forever.
  esp_core_dump_image_erase();

  return result;
}

String DeviceController::loadFile(String filename)
{
  return StorageController::loadFile(filename);
};

bool DeviceController::waitForFileCommitted(String filename, unsigned long timeoutMs)
{
  return StorageController::waitForFileCommitted(filename, timeoutMs);
}

String DeviceController::loadFileRetry(String filename, int maxAttempts, unsigned long retryDelayMs)
{
  return StorageController::loadFileRetry(filename, maxAttempts, retryDelayMs);
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

  Serial.println("[Device] config: " + ConfigParser::maskApiKeyInJson(serviceData.payload));

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

// Roadmap #127: power-rail/sleep/reboot/reset moved to PowerController - reads the same
// canonical `deviceConfig` these always read (roadmap #129), just now via parameters instead of
// member access.
void DeviceController::powerRailPrimary(bool state)
{
  PowerController::railPrimary(deviceConfig.configPin.POWER_RAIL_PRIMARY, state);
}

void DeviceController::powerRailSecondary(bool state)
{
  PowerController::railSecondary(deviceConfig.configPin.POWER_RAIL_SECONDARY, state);
}

void DeviceController::sleep()
{
  PowerController::sleep(deviceConfig.sleepSeconds, deviceConfig.sleepDeep);
}

void DeviceController::reboot()
{
  PowerController::reboot();
}

// Roadmap #127: OTA download+flash moved to OtaController. Roadmap #131: expectedSha256 passed
// through to OtaController so it can verify the flashed image before Update.end().
bool DeviceController::firmwareUpdate(String url, bool isHttps, String expectedSha256)
{
  return OtaController::update(url, isHttps, deviceConfig.servicePublicKey, deviceConfig.servicePoint, expectedSha256);
}

void DeviceController::reset()
{
  PowerController::reset();
}

// Roadmap #127: roadmap #9 store-and-forward buffer moved to StorageController.
bool DeviceController::bufferSensorDataToDisk(String payloadJson)
{
  return StorageController::bufferSensorDataToDisk(payloadJson);
}

String DeviceController::oldestBufferedSensorFile()
{
  return StorageController::oldestBufferedSensorFile();
}

void DeviceController::removeBufferedFile(String filename)
{
  StorageController::removeBufferedFile(filename);
}

// Roadmap #128: field parsing moved to ConfigParser::parse() - this stays a thin wrapper. Roadmap
// #129: mutates the single canonical `deviceConfig` (DeviceModel.h extern), the same one
// powerRailPrimary/sleep/firmwareUpdate below read.
DeviceConfig DeviceController::loadConfig(String configJson)
{
  deviceConfig = ConfigParser::parse(configJson, deviceConfig);
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
