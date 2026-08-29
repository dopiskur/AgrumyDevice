#include <NTPClient.h>
#include <ESP.h>
#include "EEPROM.h"
#include "LittleFS.h"
#include <Wire.h>
#include <ArduinoJson.h>

#include "Model/DeviceModel.h"

#include "Controller/DeviceController.h"
#include "Controller/SensorController.h"
#include "Controller/ServiceController.h"
#include "Controller/ControllerController.h"

const char *firmware = "0.1.1";
const String CONFIG_BASE = "deviceRegistration.json";
const String CONFIG_DEFAULTS = "config.json";

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

  String configDefaults = device.loadFile(CONFIG_DEFAULTS);
  delay(1000); // bare delay works around a failed-read race
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

  // TEMP: force both power rails on for testing
  device.powerRailPrimary(true);
  device.powerRailSecondary(true);
  delay(1000);

  sensor.setupSensor();    // early init for more precise measurement
  device.setupController(); // initialize time
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
  delay(deviceConfig.sleepSeconds*1000);

}
