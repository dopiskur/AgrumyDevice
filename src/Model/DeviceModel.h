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
#if defined(CONFIG_IDF_TARGET_ESP32C3)
    // ---------------------------------------------------------------------------
    // Seeed XIAO ESP32-C3 profile - SENSOR-ONLY node
    //
    // The classic-ESP32 defaults in the #else branch use GPIO25/26/27/33/34/35,
    // none of which exist on the ESP32-C3 (GPIO0-21 only). This is an ADDITIONAL
    // hardware profile - the #else branch is byte-for-byte the original and stays
    // the default for esp32dev / esp32s3usbotg.
    //
    // This board is not meant to be a controller: it is a sensor node with no
    // relay outputs. All RELAY_* stay 0, which ControllerController already treats
    // as "no relay" (`if (relayN != 0)` guards every relay write), so the control
    // path is inert here even if the server config happens to enable it.
    //
    // XIAO C3 breaks out 11 pads: GPIO2-10, 20, 21.
    //  - GPIO6/7  = default I2C (BH1750 / CCS811 / BMPx8x). Left free here, exactly
    //               as the classic profile also leaves I2C to core defaults.
    //  - GPIO2/8/9 = boot strapping pins. Left UNCONNECTED by this profile so
    //               nothing can violate the boot straps.
    //  - GPIO20/21 = UART0, reusable as GPIO because the console runs over USB-CDC
    //               (board sets ARDUINO_USB_CDC_ON_BOOT=1).
    //  - Analog inputs must stay on ADC1 (GPIO0-4) to keep reading while WiFi is
    //               on; MOIST/WaterTank -> GPIO3/GPIO4 (ADC1_CH3 / ADC1_CH4).
    //
    // Only 5 signals to place (2 power rails + DHT + 2 ADC), all on clean pins;
    // GPIO21 is left spare.

    int POWER_RAIL_PRIMARY   = 10; // D10 - clean GPIO
    int POWER_RAIL_SECONDARY = 20; // D7  - UART0 RX pad, clean (console is USB-CDC)

    int STATUS_POWER  = 0; // status LEDs are unused in firmware today and there is
    int STATUS_SENSOR = 0; // no reason to spend a pad on them on a sensor node
    int STATUS_ERROR  = 0;

    // PINOUT Sensors
    int DHT       = 5; // D3
    int TEMPSOIL  = 0; // sensor_DS18B20_temp() is an empty stub
    int MOIST     = 3; // D1 / A1 - ADC1_CH3
    int WaterTank = 4; // D2 / A2 - ADC1_CH4
    int DEPTH_RX  = 0; // no UART depth-sensor code exists
    int DEPTH_TX  = 0;
    int PH        = 0; // sensor_liquid_PH() is an empty stub

    // PINOUT Relay - none: this profile is a sensor-only node
    int RELAY_1 = 0;
    int RELAY_2 = 0;
    int RELAY_3 = 0;
    int RELAY_4 = 0;
    int RELAY_5 = 0; //UNDEFINED
    int RELAY_6 = 0; //UNDEFINED
    int RELAY_7 = 0; //UNDEFINED
    int RELAY_8 = 0; //UNDEFINED
#else
    // ---- Classic ESP32 (esp32dev) / ESP32-S3 (esp32s3usbotg) - UNCHANGED ----

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
#endif
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
    String apiAuth="";
};


struct ServiceRequest
{
    String serviceType="";
    bool isHttps=false; // set alongside serviceType by DeviceController::serviceType(), so requestPost need not re-parse the prefix
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