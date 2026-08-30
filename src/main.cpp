#include <NTPClient.h>
#include <ESP.h>
#include "EEPROM.h"
#include "LittleFS.h"
#include <Wire.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>

#include "Model/DeviceModel.h"

#include "Controller/DeviceController.h"
#include "Controller/SensorController.h"
#include "Controller/ServiceController.h"
#include "Controller/ControllerController.h"

const char *firmware = "0.1.1";
const String CONFIG_BASE = "deviceRegistration.json";
const String CONFIG_DEFAULTS = "config.json";

// Hardware watchdog (roadmap #18): reboot if a loop() cycle wedges before it completes
// (infinite loop, deadlock, a network call that never returns). Sized to clear the
// worst-case work phase - up to ~4 sequential HTTPClient calls per cycle (config sync,
// re-auth, retry, sensor push), each with the arduino-esp32 default 5 s TCP timeout,
// plus the TLS handshake - with margin. The trailing inter-cycle sleep is a bounded idle
// wait and is fed separately in loop(), so this value is independent of the server-set
// sleepSeconds.
static const uint32_t WDT_TIMEOUT_SECONDS = 90;

static JsonDocument jsonData;
static String servicePoint;

static DeviceConfig deviceConfig;
static SensorData sensorData;

static ServiceRequest serviceRequest;
static ServiceData serviceData;

static DeviceController device;
static ServiceController service;
static SensorController sensor;
static ControllerController controller;



void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("[Initialization started]");

  // format-on-fail: first boot after the SPIFFS->LittleFS switch reformats the partition.
  // A partition still holding SPIFFS bytes must fail the mount here so the format runs -
  // if it does not, flash the board with a full chip erase.
  if (!LittleFS.begin(true))
  {
    Serial.println("[Main] LittleFS mount/format FAILED");
  }
  Serial.printf("[FS] LittleFS total=%u used=%u bytes\n", LittleFS.totalBytes(), LittleFS.usedBytes());
  EEPROM.begin(512);
  delay(500);

  String configRegistration = device.loadFile(CONFIG_BASE);
  delay(1000); // bare delay works around a failed-read race
  if (configRegistration.isEmpty())
  {
    Serial.println("[Main] Registration file not found, starting initialization...");
    device.initializeDevice(); // blocks in the Agrumy_<mac> portal, then reboots
  }

  device.initializeWifi();
  delay(1000);

  // Config-integrity crash-loop guard: 3 config-triggered reboots in a row, each within 60s of
  // its own boot, means the last config update is likely the cause - load the backup instead of
  // repeating the same crash forever. See ServiceController::apiConfig's notePendingConfigReboot,
  // the only place that feeds this counter.
  bool rollbackToBackup = device.consumeRollbackTrigger();
  // Roadmap #37 - always consumed regardless of rollbackToBackup; which one wins is decided once
  // WiFi/auth are up below (a rollback boot confirms CrashLoopRollback, never ConfigApplied, even
  // though both flags are set together on the reboot that triggered the rollback).
  bool configJustApplied = device.consumeConfigAppliedPending();
  String configDefaults = rollbackToBackup ? device.loadFile("config.json.bak") : device.loadFile(CONFIG_DEFAULTS);
  delay(1000); // bare delay works around a failed-read race
  if (rollbackToBackup)
  {
    if (configDefaults.isEmpty())
    {
      Serial.println("[Main] Rollback requested but config.json.bak is missing/empty - falling back to config.json");
      configDefaults = device.loadFile(CONFIG_DEFAULTS);
    }
    else
    {
      Serial.println("[Main] Repeated rapid reboots after a config update detected - rolling back to config.json.bak");
      device.saveFile(configDefaults, CONFIG_DEFAULTS); // persist so the rollback sticks even if this boot reboots again for any other reason
    }
  }
  if (configDefaults.isEmpty())
  {
    Serial.println("[Main] Config file not found, starting initialization...");
    device.registerDevice(configRegistration);
  }

  deviceConfig = device.loadConfig(configDefaults);
  device.deviceConfig = deviceConfig;
  sensor.deviceConfig = deviceConfig;
  service.deviceConfig = deviceConfig;
  controller.deviceConfig = deviceConfig;

  serviceRequest.serviceType = device.serviceType(deviceConfig.deviceTypeServiceID, serviceRequest.isHttps);
  serviceRequest.servicePoint = deviceConfig.servicePoint;

  service.serviceRequest = serviceRequest;
  sensor.serviceRequest = serviceRequest;

  // Roadmap #28/#37: confirm the outcome of the reboot that just happened back to the server, now
  // that deviceConfig/serviceRequest are populated (too early to do this above, where the rollback
  // branch runs). A rollback boot always wins over configJustApplied - both flags are set together
  // on the reboot that triggered the rollback (see notePendingConfigReboot()), but that boot is
  // applying the OLD backup, not the new config configJustApplied refers to.
  if (rollbackToBackup || configJustApplied)
  {
    // apiAuthenticate() takes serviceRequest by value, so its internal apiId/apiKey/endpoint
    // mutations never reach this copy - re-set apiId here before reusing serviceRequest below.
    service.apiAuthenticate(deviceConfig, serviceRequest, device);
    serviceRequest.header.apiId = deviceConfig.apiId;
    if (rollbackToBackup)
    {
      // The threshold in consumeRollbackTrigger() is a fixed compile-time 3, and it fires the
      // moment the count first reaches it (called every boot) - so "3" is always accurate here,
      // even though the counter itself is already reset by the time this message is composed.
      service.pushEvent(serviceRequest, "CrashLoopRollback", "3 consecutive early reboots detected");
    }
    else
    {
      service.pushEvent(serviceRequest, "ConfigApplied", "version=" + String(deviceConfig.configVersion));
    }
  }

  // TEMP: force both power rails on for testing
  device.powerRailPrimary(true);
  device.powerRailSecondary(true);
  delay(1000);

  sensor.setupSensor();    // early init for more precise measurement
  device.setupController(); // initialize time

  // Arm the watchdog only now that setup (incl. the blocking WiFi portal / registration
  // path) is done - those legitimately take longer than one loop cycle. The arduino-esp32
  // core already runs a 5 s task WDT watching the CPU0 idle task; tear that down and
  // re-init at our own timeout with panic+reboot before subscribing the loop task,
  // because esp_task_wdt_init() is a no-op if the WDT is already initialized.
  esp_task_wdt_deinit();
  esp_task_wdt_init(WDT_TIMEOUT_SECONDS, true); // true: panic-handler reboot on timeout
  esp_task_wdt_add(NULL);                       // watch the Arduino loop task

  Serial.println("[Initialization] Finished: ");
}

void loop()
{
  Serial.println("[Loop]-----> Start <-----[Loop]");
  Serial.print("[Heap] loop start: "); // TEMPORARY DIAGNOSTIC: heap-leak investigation
  Serial.println(ESP.getFreeHeap());
  if (deviceConfig.batteryEnabled)
  {
    device.powerRailPrimary(true);
    device.powerRailSecondary(true);
  }

  service.apiConfig(deviceConfig, serviceRequest, device);

  if (deviceConfig.enabled) {
    sensor.buildSensorData(deviceConfig);
  }

  if (deviceConfig.batteryEnabled)
  {
    device.powerRailPrimary(false);
    device.powerRailSecondary(false);
  }
  
  Serial.println("[Loop]-----> END <-----[Loop]");
  Serial.println("");

  // A full cycle finished without wedging - feed the watchdog. Anything that hangs inside
  // apiConfig() / buildSensorData() never reaches this point, so the reboot backstop stays
  // effective against a real stall.
  esp_task_wdt_reset();

  // Inter-cycle pause. It is a bounded idle wait, not work that can hang, so keep petting
  // the watchdog through it in steps shorter than the timeout - otherwise a server-set
  // sleepSeconds longer than WDT_TIMEOUT_SECONDS would look like a stall and reboot the
  // device every cycle.
  uint32_t sleepRemaining = (uint32_t)deviceConfig.sleepSeconds * 1000UL;
  const uint32_t sleepStep = WDT_TIMEOUT_SECONDS * 1000UL / 3;
  while (sleepRemaining > 0)
  {
    uint32_t chunk = sleepRemaining < sleepStep ? sleepRemaining : sleepStep;
    delay(chunk);
    esp_task_wdt_reset();
    sleepRemaining -= chunk;
  }
}
