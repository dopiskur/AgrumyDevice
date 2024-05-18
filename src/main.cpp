// ESP32 libraries
#include <NTPClient.h>
#include <ESP.h> //ESP sleep
#include "EEPROM.h"
#include "SPIFFS.h"      // File system library
#include <Wire.h>        // I2C scanner
#include <ArduinoJson.h> // JsonDocument



// Model LIB
#include "Model/DeviceModel.h"
#include "Model/SensorModel.h"

// Controller LIB
#include "Controller/DeviceController.h"
#include "Controller/SensorController.h"
#include "Controller/ServiceController.h"
#include "Controller/ControllerController.h"

// put function declarations here:
const char *firmware = "0.1.1";
const String CONFIG_BASE = "deviceRegistration.json";
const String CONFIG_DEFAULTS = "config.json";

static JsonDocument jsonData;
static String servicePoint;

static DeviceConfig deviceConfig; // initialize data structure
static SensorData sensorData;     // initialize data structure

static ServiceRequest serviceRequest; // initialize data structure
static ServiceData serviceData;       // initialize data structure

// Controller
static DeviceController device;   // initialize functions
static ServiceController service; // initialize functions
static SensorController sensor;
static ControllerController controller;



void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("[Initialization started]");

  SPIFFS.begin();
  EEPROM.begin(512);
  delay(500);

  // TEST FORMAT
  // SPIFFS.format();
  //

  // READ CONFIG
  String configRegistration = device.loadFile(CONFIG_BASE);
  delay(1000); // Solving problem with failed read by adding delay
  if (configRegistration == NULL)
  {
    Serial.println("[Main] Registration file not found, starting initialization...");
    device.initializeDevice(); // build config
  }


  // Start WiFi
  device.initializeWifi();
  delay(1000);

  String configDefaults = device.loadFile(CONFIG_DEFAULTS); // reading default config from SPIFFS
  delay(1000);                                              // Solving problem with failed read by adding delay
  if (configDefaults == NULL)
  {
    Serial.println("[Main] Config file not found, starting initialization...");
    device.registerDevice(configRegistration); // build config
  }

  

  // Loading Config
  deviceConfig = device.loadConfig(configDefaults);
  device.deviceConfig = deviceConfig;  // sending values to device
  sensor.deviceConfig = deviceConfig;  // sending values to sensor
  service.deviceConfig = deviceConfig; // sending values to service
  controller.deviceConfig = deviceConfig;

  // Initialize serviceRequest defaults
  serviceRequest.serviceType = device.serviceType(deviceConfig.deviceTypeServiceID);
  serviceRequest.servicePoint = deviceConfig.servicePoint;

  service.serviceRequest = serviceRequest;
  sensor.serviceRequest = serviceRequest;

// Start power rail by default
  device.powerRailPrimary(true);
  delay(1000);

  
  sensor.setupSensor(); // initialize sensors early for more precise mesurement
  device.setupController(); // initialize time
  Serial.println("[Initialization] Finished: ");
}

void loop()
{
  Serial.println("[Loop]-----> Start <-----[Loop]");
  // Turn ON power rail if battery enabled
  if (deviceConfig.batteryEnabled == true)
  {
    device.powerRailPrimary(true);
    device.powerRailSecondary(true);
  }

  service.apiConfig(deviceConfig, serviceRequest); // check config

  if(deviceConfig.enabled==true){
    sensor.buildSensorData(deviceConfig);
  }
  
  // Turn OFF power rail if battery enabled
  if (deviceConfig.batteryEnabled == true)
  {
    device.powerRailPrimary(false);
    device.powerRailSecondary(false);
  }
  
  Serial.println("[Loop]-----> END <-----[Loop]");
  Serial.println("");
  delay(60000);

}
