#ifndef DATASTRUCTURE_H
#define DATASTRUCTURE_H
#include "Arduino.h"

struct DeviceDefaults {
    String servicePoint = "api.agrumy.com";
    int serviceType = 1; // 0 http, 1 https, 2 mqtt
};


struct DeviceRegistration
{
  char userLogin[128];
  char devicePin[8];
  char servicePoint[256];
  bool initialize=false;
};

struct EventLog
{
    bool error = false;
    int errorCode = 0;
    String errorData ="";
};

struct ConfigPin // default values, cannot be changed during the setup phase
{
    // The Seeed XIAO ESP32-C3 sensor-only profile (a CONFIG_IDF_TARGET_ESP32C3 branch here,
    // plus its platformio.ini env and CI matrix entry) was removed 2026-08-31 by explicit
    // decision - recover it from git history (added in b58876b) if a C3-class sensor node
    // ever returns to the lineup.

    // ---- Classic ESP32 (esp32dev) / ESP32-S3 (esp32s3usbotg) ----

    // PINOUT general
    int POWER_RAIL_PRIMARY=2;
    int POWER_RAIL_SECONDARY=15;

    int STATUS_POWER=4;
    int STATUS_SENSOR=5;
    int STATUS_ERROR=16; // RX2 pin

    // PINOUT Sensors
    int DHT=19;      // DHT sensor
    int TEMPSOIL=5; // Soil temperature
    int MOIST=34;
    int WaterTank=35;
    int DEPTH_RX=13;
    int DEPTH_TX=12;
    int PH=33;

    // PINOUT Relay
    int RELAY_1=14;
    int RELAY_2=27;
    int RELAY_3=26;
    int RELAY_4=25;
    int RELAY_5=0; //UNDEFINED
    int RELAY_6=0; //UNDEFINED
    int RELAY_7=0; //UNDEFINED
    int RELAY_8=0; //UNDEFINED
};

struct ModuleEnabled
{
    bool moisture; // analog
    bool waterLevel;   // Analog water level
    bool dht;          // temperature, moisture
    bool bmp180;       // temperature, pressure
    bool bmp280;       // temperature, pressure
    bool bme280;       // temperature, pressure, moisture
    bool ds18b20;      // temperature
    bool ccs811;       // CO2, TVOC
    bool bh1750;       // Light intensity
    bool liquidPH;     // PH sensor
    bool AJSR04M;      // Digital water level
    bool battery;      // Battery Sensor, voltage
    bool camera;

    bool rtc;   // Clock module
    bool relay; // relay 
};

struct RelayFunction
{

    String relay1; // valve, pump, fan, heating
    String relay2; // valve, pump, fan, heating
    String relay3; // valve, pump, fan, heating
    String relay4; // valve, pump, fan, heating

    // Standalone Relay module without sensors
    String relay5; // valve, pump, fan, heating
    String relay6; // valve, pump, fan, heating
    String relay7; // valve, pump, fan, heating
    String relay8; // valve, pump, fan, heating
};

struct SensorType
{
    String battery;
    String temperature; // DHT, BMP180, BME280,
    String humidity;    // DHT, BME280,
    String barometer;  // BMP180, BME280
    String waterTank;   // AJSR04M, waterLevel
    
};


struct ConfigSensor
{
    int sensorBattery;
    int sensorTemp;
    int sensorTempSoil;
    int sensorHumid;
    int sensorMoist;
    int sensorLight;
    int sensorCo2;
    int sensorTvoc;
    int sensorBarometer;
    int sensorPH;
    int sensorRainLevel;
    int sensorWaterLevel;
    int sensorWind;
};

struct ConfigController
{

    double tempLow;
    double tempHigh;
    double humidLow;
    double humidHigh;
    int moistLow;
    int moistHigh;
    int lightLow;
    int lightHigh;
    int waterLow;
    int waterHigh;

    // Hysteresis (dead zone) margins for the four threshold-based relay functions - prevents
    // chattering when a sensor value sits right at its threshold. Server-configurable (roadmap
    // #10), sent as part of deviceConfigController on every config sync. Default member
    // initializers below are the fallback used if the server doesn't send these keys (older
    // API) - loadConfig() falls back to whatever value is already here, so a config sync never
    // resets a device to 0.
    double waterLevelHysteresis = 5.0;   // same raw unit as waterLevel/waterLow
    double temperatureHysteresis = 1.0;  // deg C
    double humidityHysteresis = 5.0;     // percent
    double lightHysteresis = 20.0;       // same raw unit as light/lightLow

    int ventilationIntervalEnabled;
    int ventilationInterval;
    int ventilationIntervalLength;
    int lightIntervalEnabled;
    int lightInterval;
    int lightIntervalLength;
    int heatingIntervalEnabled;
    int heatingInterval;
    int heatingIntervalLength;
    int waterPumpIntervalEnabled;
    int waterPumpInterval;
    int waterPumpIntervalLength;

    // Roadmap #39: a third relay-control mode alongside threshold and interval above - "be on
    // during this wall-clock window on these days", independent of any sensor reading. Evaluated
    // by ActuatorController::scheduleRelayFunction against LOCAL time, derived on-device from
    // DeviceConfig.utcOffsetSeconds (no timezone database needed here - see that field's comment).
    // daysOfWeek is a 7-bit mask matching C's tm_wday (bit 0 = Sunday .. bit 6 = Saturday). start/
    // duration are seconds since local midnight - v1 does not support a window crossing midnight
    // (enforced server-side, DeviceApiController.ScheduleWindowError).
    int ventilationScheduleEnabled;
    int ventilationScheduleDaysOfWeek;
    int ventilationScheduleStart;
    int ventilationScheduleDuration;
    int lightScheduleEnabled;
    int lightScheduleDaysOfWeek;
    int lightScheduleStart;
    int lightScheduleDuration;
    int heatingScheduleEnabled;
    int heatingScheduleDaysOfWeek;
    int heatingScheduleStart;
    int heatingScheduleDuration;
    int waterPumpScheduleEnabled;
    int waterPumpScheduleDaysOfWeek;
    int waterPumpScheduleStart;
    int waterPumpScheduleDuration;

    int relayEnabled;
    int relay1;
    int relay2;
    int relay3;
    int relay4;
    int relay5;
    int relay6;
    int relay7;
    int relay8;
};


struct DeviceConfig
{
    // User input
    String WifiSSID;
    String WifiPassword;
    String userLogin; // Device registration
    String devicePin; // Device registration

    // Service config
    int configVersion;

    int tenantID;
    int deviceID;
    int deviceUnitID;
    int deviceUnitZoneID;
    int deviceTypeServiceID;

    String apiId;
    String apiKey;
    String servicePoint;
    String servicePublicKey;

    int sleepSeconds;
    bool sleepDeep;

    // Roadmap #39: current UTC offset in seconds (positive east of UTC) for the server's configured
    // schedule timezone, refreshed on every config sync - lets scheduleRelayFunction() compute local
    // day-of-week/time-of-day from the NTP epoch with plain integer math, no on-device IANA/DST
    // database. 0 (UTC) both by default and whenever no schedule timezone is configured server-side.
    int utcOffsetSeconds = 0;

    bool deviceSensorEnabled;
    bool deviceControllerEnabled;
    bool batteryEnabled;
    bool enabled;
    bool debug;           // 0 serial print disabled, 1 serial print enabled
    bool reboot;
    bool reset;
    bool firmwareUpdate; // 0 no update, 1 update available
    String firmwareVersion; // roadmap #3 (OTA): newest published version for this device type, "" if none
    String firmwareUrl;     // roadmap #3 (OTA): .bin download URL, paired with firmwareVersion

    ConfigSensor configSensor;
    ConfigController configController;
    ConfigPin configPin;
    
    EventLog eventlog;
};



struct SensorData
{
    int tenantID;
    int deviceID;
    int deviceUnitID;
    int deviceUnitZoneID;

    String battery;
    String temperature;
    String temperatureSoil;
    String humidity;
    String moisture;
    String light;
    String co2;
    String tvoc;
    String barometer;
    String liquidPH;
    String rainLevel;
    String waterLevel;
    String wind;
    String dateCreated;
    EventLog eventlog;
};

// HTTP/MQTT payload
struct ServiceData
{   
    String payload=""; 
    EventLog eventlog;
};


struct ServiceHeader{
    String apiId="";
    String apiKey="";
};


struct ServiceRequest
{
    String serviceType="";
    bool isHttps=false; // set alongside serviceType by DeviceController::serviceType(), so requestPost need not re-parse the prefix
    String servicePoint="";
    String endpoint="";
    ServiceHeader header;

    // Roadmap #110: single source of truth for the full request URL - was duplicated as raw
    // string concatenation at both call sites (DeviceController::registerDevice,
    // ServiceController::requestPost).
    String url() const { return serviceType + servicePoint + endpoint; }
};




struct ServiceEndpoint
{
    String apiRegister = "/api/Device/Register";
    String apiConfig = "/api/Device/Config";
    String apiAuthenticate = "/api/Device/Authenticate";
    String apiEvent = "/api/Device/Event"; // roadmap #28

    String apiSensorDataPost="/api/SensorData";
    String apiSensorDataGet="";

};



#endif