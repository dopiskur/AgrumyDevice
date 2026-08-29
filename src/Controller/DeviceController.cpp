#include "arduino.h"
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

static DeviceDefaults deviceDefaults;
static DeviceConfig deviceConfig;
static ServiceEndpoint serviceEndpoint;

DeviceRegistration deviceRegistration;
JsonDocument config;

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

// LittleFS lives in the partition labelled "spiffs" (default scheme, no board_build.partitions):
// esp32dev / seeed_xiao_esp32c3 (4MB, default.csv) = 1408 KB; esp32s3usbotg (8MB, default_8MB.csv)
// = 1536 KB. It is a separate flash region from the OTA app partitions (ota_0/ota_1), so roadmap
// #3 OTA never touches it. Stored today: config.json ~2.2 KB + deviceRegistration.json ~0.15 KB
// => < 2% used, leaving ~1.35-1.5 MB for the roadmap #9 store-and-forward queue.
void DeviceController::saveFile(String data, String filename)
{
  String path = "/" + filename;
  File file = LittleFS.open(path, "w");

  if (!file)
  {
    Serial.println("Failed to open file for write, formating device");
    LittleFS.format();
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
  delay(1000);
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

  String serviceURL = serviceRequest.serviceType + serviceRequest.servicePoint + serviceRequest.endpoint; // TODO: extract as a constant
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

  Serial.println("[Device] config: " + serviceData.payload);

  saveFile(serviceData.payload, "config.json");
  delay(1000);
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

void DeviceController::sleep()
{
#define uS_TO_S_FACTOR 1000000 /* Conversion factor for micro seconds to seconds */
  int TIME_TO_SLEEP = deviceConfig.sleepSeconds;
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

  if (deviceConfig.sleepDeep)
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
    // Validate against the embedded CA bundle, same as ServiceController::requestPost().
    secureClient.setCACert(nullptr);
    secureClient.setCACertBundle(rootca_crt_bundle_start);
    http.begin(secureClient, url);
  }
  else
  {
    http.begin(url);
  }

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

String DeviceController::saveValusOnError()
{
  // not implemented: should buffer JSON results in a list and flush every 10 to disk
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
    deviceConfig.eventlog.errorCode = 20; // 10 is reserved for deserialize fail
    deviceConfig.eventlog.errorData = error.c_str();

    return deviceConfig;
  }

  String servicePoint = config["servicePoint"];
  String servicePublicKey = config["servicePublicKey"];
  String apiId = config["apiId"];
  String apiKey = config["apiKey"];

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
  deviceConfig.firmwareVersion = config["firmwareVersion"] | String(""); // roadmap #3 (OTA)
  deviceConfig.firmwareUrl = config["firmwareUrl"] | String("");

  if (deviceConfig.deviceSensorEnabled)
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
