#ifndef DATASTRUCTURE_H
#define DATASTRUCTURE_H
#include "Arduino.h"

// mozda dodati uptime?
// D15 pin ostavi za hard reset na gumb


struct DeviceDefaults {
    String servicePoint = "api.agrumy.com";
    int serviceType = 1; // 0 http, 1 https, 2 mqtt
};


struct DeviceRegistration
{
  char userLogin[128];
  char devicePin[8];
  char servicePoint[256];
  bool intialize=false;
};

struct EventLog
{
    bool error = false;
    int errorCode = 0;
    String errorData ="";
};

struct ConfigPin // default values, can not be changed during setup phaze
{

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
    // modules
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
    // values
    String battery;
    String temperature; // DHT, BMP180, BME280,
    String humidity;    // DHT, BME280,
    String barometer;  // BMP180, BME280
    String waterTank;   // AJSR04M, waterLevel
    
};

struct ModuleDefaults
{
    int moisture_low;
    int moisture_high;
    int light_low;
    int light_high;
    int water_low;
    int water_high;
    int temp_low;
    int temp_high;
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

    int ventilationIntervalEnabled;
    int ventilationInterval;
    int ventilationIntervalLenght;
    int lightIntervalEnabled;
    int lightInterval;
    int lightIntervalLenght;
    int heatingIntervalEnabled;
    int heatingInterval;
    int heatingIntervalLenght;
    int waterPumpIntervalEnabled;
    int waterPumpInterval;
    int waterPumpIntervalLenght;

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


// Structure Config
struct DeviceConfig
{
    // User input
    String WifiSSID; 
    String WifiPassword; 
    String userLogin; // Device registration
    String devicePin; // Device registration

    // Service config
    int configVersion;

    // Config
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
    bool deviceSensorEnabled;
    bool deviceControllerEnabled;
    bool batteryEnabled;
    bool enabled;
    bool debug;           // 0 serial print disabled, 1 serial print enabled
    bool reboot;
    bool reset;
    bool firmwareUpdate; // 0 no update, 1 update available

    ConfigSensor configSensor;
    ConfigController configController;
    ConfigPin configPin;
    
    EventLog eventlog;

    //ModuleEnabled moduleEnabled;
    //SensorType sensorType;
    //ModuleDefaults moduleDefaults;   
    //RelayFunction relayFunction;
    
};



// Structure Sensor
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
    String apiAuth="";
};


struct ServiceRequest
{
    String serviceType="";
    String servicePoint="";
    String endpoint="";
    ServiceHeader header;
};




struct ServiceEndpoint
{
    String apiRegister = "/api/Device/Register";
    String apiConfig = "/api/Device/Config";
    String apiAuthenticate = "/api/Device/Authenticate";

    String apiSensorDataPost="/api/SensorData";   
    String apiSensorDataGet="";
    
};



#endif