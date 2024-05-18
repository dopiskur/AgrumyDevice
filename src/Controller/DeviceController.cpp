#include "arduino.h"
#include <WiFi.h> //wifi connection WiFi.begin(ssid, password)
#include <EEPROM.h>
#include "SPIFFS.h"
#include "FS.h"
#include "WiFiManager.h"

#include "DeviceController.h"
#include "ServiceController.h"

#include <NTPClient.h> // Time library
#include <WiFiUdp.h>   // Time library requriment

static ServiceController service; // Static rjesava problem s konfliktom u mainu

// Models
static DeviceDefaults deviceDefaults;
static DeviceConfig deviceConfig;
static ServiceEndpoint serviceEndpoint;

// MAC ID, ovo mozda vise nije potrebno jer imamo chipid = ESP.getEfuseMac(), ali i tako moram preradit MAC u string bez dvotocke;
DeviceRegistration deviceRegistration;
JsonDocument config;

// Time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

String DeviceController::getDateTime()
{

  time_t epochTime = timeClient.getEpochTime();
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

void DeviceController::saveFile(String data, String filename)
{
  String path = "/" + filename;
  File file = SPIFFS.open(path, "w");

  if (!file)
  {
    Serial.println("Failed to open file for write, formating device");
    SPIFFS.format();
    return;
  }

  Serial.print("Saving file: ");
  Serial.println(filename);
  file.print(data);

  file.close();
};

String DeviceController::loadFile(String filename)
{
  String path = "/" + filename;
  File file = SPIFFS.open(path);
  String data;

  if (!file)
  {
    Serial.println("Failed to open file for write");
    // napravi format!!!
    data = "error";
  }
  else
  {
    Serial.print("Reading file: ");
    Serial.println(filename);
    while (file.available())
    {
      data += char(file.read());
    }
    file.close();
  }

  return data;
};

void DeviceController::initializeWifi()
{

  WiFi.mode(WIFI_STA);
  WiFiManager wifiManager;

  // wifiManager.autoConnect(deviceConfig.WifiSSID.c_str(), deviceConfig.WifiPassword.c_str());
  wifiManager.autoConnect();

  // wifiManager.resetSettings();
}

void DeviceController::initializeDevice()
{
  Serial.println("[Device]: initializeDevice");
  // Start Wifi
  WiFi.mode(WIFI_STA);

  // Connect to WiFi
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

  // Saving backup values to EEPROM
  strncpy(deviceRegistration.userLogin, userLogin.getValue(), 128); // ovo je nepotrebno, krc zesci, moze odmah u string, ali manje problema
  strncpy(deviceRegistration.devicePin, userPin.getValue(), 8);
  strncpy(deviceRegistration.servicePoint, servicePoint.getValue(), 256);

  deviceRegistration.intialize = true;

  // SET DEFAULT SERVICE POINT
  // if (deviceRegistration.servicePoint == NULL)
  //{
  //  String serviceDefaultPoint= "api.agrumy.com";
  //  strcpy(deviceRegistration.servicePoint, serviceDefaultPoint.c_str());
  //}

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
  delay(1000);
  reboot();
};

void DeviceController::registerDevice(String configRegistration)
{ // register device on API
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

  // Prepare request
  String servicePoint = config["servicePoint"];
  ServiceRequest serviceRequest;
  serviceRequest.serviceType = serviceType(deviceConfig.deviceTypeServiceID);
  serviceRequest.servicePoint = servicePoint;
  serviceRequest.endpoint = serviceEndpoint.apiRegister;

  String serviceURL = serviceRequest.serviceType + serviceRequest.servicePoint + serviceRequest.endpoint; // OVO MORAMO NEGDJE EKSTRAKTAT KAO KONSTANTU
  Serial.println("[Device] Fetching config from " + serviceURL);
  String exitData;
  serializeJsonPretty(payload, exitData);          // for debug only
  Serial.println("[Device] Payload: " + exitData); // for debug only
  ServiceData serviceData = service.requestPost(payload, serviceRequest);

  // Check for error
  if (serviceData.eventlog.errorCode == 401)
  {
    Serial.println("[Device] Wrong user or pin, try again. Error: " + serviceData.eventlog.errorCode);
    reset();
  }

  Serial.println("[Device] config: " + serviceData.payload);

  // Build and save config
  // String data = buildConfig(deviceConfig);
  // Serial.println("[Device] config: "+ data);
  saveFile(serviceData.payload, "config.json");
  delay(1000);
  reboot();
}

String DeviceController::macAddr() // get MAC address as ID
{
  byte mac[6];
  WiFi.macAddress(mac);
  char macAddr[18];
  sprintf(macAddr, "%2X%2X%2X%2X%2X%2X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  for (int i = 0; i < 18; i++) // zamjene razmaka sa nulom
  {
    if (macAddr[i] == ' ')
      macAddr[i] = '0';
  }
  // Serial.println(macAddr);
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
  delay(500); // delay for startup
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
  delay(500); // delay for startup
}

void DeviceController::sleep()
{
#define uS_TO_S_FACTOR 1000000 /* Conversion factor for micro seconds to seconds */
  int TIME_TO_SLEEP = deviceConfig.sleepSeconds;
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

  if (deviceConfig.sleepDeep == 1)
  {
    Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Seconds");
    Serial.println("Going to sleep now");
    Serial.flush();
    esp_deep_sleep_start();
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

void DeviceController::reset()
{
  SPIFFS.format();
  ESP.restart();
}

String DeviceController::saveValusOnError()
{
  // ovdje treba biti lista
  // https://appdividend.com/2022/03/22/cpp-list/#:~:text=C%2B%2B%20List%20is%20a%20built,Template%20Library)%20in%20C%2B%2B.;
  // not implemented, should add json results into list, write every 10x results on disk
  return "list";
}

DeviceConfig DeviceController::loadConfig(String configJson)
{
  Serial.println("[Device] Load config: " + configJson);

  DeserializationError error = deserializeJson(config, configJson);

  if (error)
  {
    Serial.print("[Device] Load Config; deserializeJson() failed: ");
    Serial.println(error.c_str());
    deviceConfig.eventlog.error = true;
    deviceConfig.eventlog.errorCode = 20; // error code 10 will be used for deserialze fail
    deviceConfig.eventlog.errorData = error.c_str();

    return deviceConfig;
  }

  // Config to string
  String servicePoint = config["servicePoint"];
  String servicePublicKey = config["servicePublicKey"];
  String apiId = config["apiId"];
  String apiKey = config["apiKey"];

  // Set config
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
  deviceConfig.deviceSensorEnabled = config["deviceSensorEnabled"];
  deviceConfig.deviceControllerEnabled = config["deviceControllerEnabled"];
  deviceConfig.batteryEnabled = config["batteryEnabled"];
  deviceConfig.enabled = config["enabled"];
  deviceConfig.debug = config["debug"];
  deviceConfig.reboot = config["reboot"];
  deviceConfig.reset = config["reset"];
  deviceConfig.firmwareUpdate = config["firmwareUpdate"];

  if (deviceConfig.deviceSensorEnabled == true)
  {
    JsonObject deviceConfigSensor = config["deviceConfigSensor"];

    deviceConfig.configSensor.sensorBattery = deviceConfigSensor["sensorBattery"];
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

  if (deviceConfig.deviceControllerEnabled == true)
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

    deviceConfig.configController.ventilationIntervalEnabled = deviceConfigController["ventilationIntervalEnabled"];
    deviceConfig.configController.ventilationInterval = deviceConfigController["ventilationInterval"];
    deviceConfig.configController.ventilationIntervalLenght = deviceConfigController["ventilationIntervalLenght"];
    deviceConfig.configController.lightIntervalEnabled = deviceConfigController["lightIntervalEnabled"];
    deviceConfig.configController.lightInterval = deviceConfigController["lightInterval"];
    deviceConfig.configController.lightIntervalLenght = deviceConfigController["lightIntervalLenght"];
    deviceConfig.configController.heatingIntervalEnabled = deviceConfigController["heatingIntervalEnabled"];
    deviceConfig.configController.heatingInterval = deviceConfigController["heatingInterval"];
    deviceConfig.configController.heatingIntervalLenght = deviceConfigController["heatingIntervalLenght"];
    deviceConfig.configController.waterPumpIntervalEnabled = deviceConfigController["waterPumpIntervalEnabled"];
    deviceConfig.configController.waterPumpInterval = deviceConfigController["waterPumpInterval"];
    deviceConfig.configController.waterPumpIntervalLenght = deviceConfigController["waterPumpIntervalLenght"];

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

String DeviceController::serviceType(int deviceServiceTypeID)
{
  String serviceType;
  // String certPublicKey = deviceConfig.servicePublicKey; // not needed

  switch (deviceServiceTypeID)
  {
  case 0:
    serviceType = "http://";
    break;
  case 1:
    serviceType = "https://";
    break;
  case 2:
    serviceType = "mqtt://";
    break;
  default:
    serviceType = "https://";
    break;
  }

  return serviceType;
}
