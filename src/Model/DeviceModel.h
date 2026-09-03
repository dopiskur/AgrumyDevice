#ifndef DATASTRUCTURE_H
#define DATASTRUCTURE_H
#include "Arduino.h"
#include "../Logic/RelayLogic.h" // ScheduleWindow/MAX_SCHEDULE_SLOTS_PER_FUNCTION (roadmap #115) - plain C++, no Arduino dependency of its own

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

// Roadmap #34: mirrors api.Models.CommandActionType - Reboot/ForceOTA/ForceConfigSync, raw
// integers on the wire (same server->firmware convention as relay function/service type), not
// strings. SelfTest intentionally excluded from v1 - no device-side concept exists.
enum CommandActionType
{
    COMMAND_REBOOT = 1,
    COMMAND_FORCE_OTA = 2,
    COMMAND_FORCE_CONFIG_SYNC = 3,
};

// Roadmap #34: the minimal shape ridden along in the Config/Register response (api.Models.
// PendingCommand) - present=false when the server has nothing queued for this device.
struct PendingCommand
{
    bool present = false;
    int idDeviceCommand = 0;
    int actionType = 0;
    String expiresAt = "";
};

struct ConfigPin // default values, cannot be changed during the setup phase
{
    // The Seeed XIAO ESP32-C3 sensor-only profile (a CONFIG_IDF_TARGET_ESP32C3 branch here,
    // plus its platformio.ini env and CI matrix entry) was removed 2026-08-31 by explicit
    // decision - recover it from git history (added in b58876b) if a C3-class sensor node
    // ever returns to the lineup.

    // Roadmap #149: which branch below compiles in is selected by the AGRUMY_KIT_* define set in
    // this env's platformio.ini build_flags (kc868-a6/esp32-s3-relay-6ch), separate from the
    // AGRUMY_KIT string sent in the heartbeat (ServiceController.cpp) - the define picks the
    // pinout at compile time, the string is what the server's deviceTypeKit lookup matches on.
#if defined(AGRUMY_KIT_KC868_A6)
    // ---- KinCony KC868-A6 ----
    // Sourced from KinCony's public schematic/forum pin-define thread (kincony.com/forum,
    // tid=1962) and the community KC868-A6 ESPHome device config (devices.esphome.io) - NOT
    // physically verified against a real board by this project. Confirm on real hardware before
    // the first field deploy (roadmap #149's own caution: "theoretically added" is not the same
    // as "supported").
    //
    // Relays sit behind a PCF8574 I2C expander, not direct GPIO (see RELAY_I2C_* below and
    // RelayIO.h) - RELAY_1..6 here are PCF8574 BIT INDICES (0-5), not GPIO numbers.
    int POWER_RAIL_PRIMARY=0; //UNDEFINED - no documented equivalent of the reference board's sensor power rail on this kit
    int POWER_RAIL_SECONDARY=0; //UNDEFINED

    int STATUS_POWER=0; //UNDEFINED - no documented status-LED scheme matching the reference board's
    int STATUS_SENSOR=0; //UNDEFINED
    int STATUS_ERROR=0; //UNDEFINED

    // PINOUT Sensors - KC868-A6 is a home-automation relay board, not an agri-sensor board; only
    // its shared DS18B20/DHT/LED header is documented (GPIO32/33), the rest have no pin on this kit.
    int DHT=32;
    int TEMPSOIL=33;
    int MOIST=0; //UNDEFINED
    int WaterTank=0; //UNDEFINED
    int DEPTH_RX=0; //UNDEFINED
    int DEPTH_TX=0; //UNDEFINED
    int PH=0; //UNDEFINED
    int BATTERY_ADC=0; //UNDEFINED - mains-powered board, no documented battery-monitoring pin

    // PINOUT Relay - PCF8574 output-expander bit indices (address in RELAY_I2C_ADDRESS below),
    // sequential P0..P5 per the community ESPHome config.
    int RELAY_1=0;
    int RELAY_2=1;
    int RELAY_3=2;
    int RELAY_4=3;
    int RELAY_5=4;
    int RELAY_6=5;
    int RELAY_7=0; //UNDEFINED - only 6 relay channels on this kit
    int RELAY_8=0; //UNDEFINED
#elif defined(AGRUMY_KIT_ESP32S3_RELAY6CH)
    // ---- Waveshare ESP32-S3-Relay-6CH ----
    // Sourced from the Waveshare wiki, CNX Software's coverage, and a working community ESPHome
    // config (github.com/ryansch/esphome-config) that agree on all six relay pins - NOT physically
    // verified against a real board by this project. Confirm on real hardware before the first
    // field deploy (same caution as KC868-A6 above).
    //
    // Direct GPIO, same digitalWrite/pinMode model as esp32dev/esp32s3usbotg - no I2C expander on
    // this kit (RELAY_I2C_ADDRESS stays 0 below).
    int POWER_RAIL_PRIMARY=0; //UNDEFINED - no documented equivalent on this kit
    int POWER_RAIL_SECONDARY=0; //UNDEFINED

    int STATUS_POWER=0; //UNDEFINED - board has its own RGB status LED (GPIO38) and buzzer (GPIO21),
    int STATUS_SENSOR=0; //UNDEFINED  neither matching Agrumy's tri-LED status scheme, left unwired
    int STATUS_ERROR=0; //UNDEFINED  here rather than guessing a mapping

    // PINOUT Sensors - this is a pure relay/actuator board, no sensor headers documented at all.
    int DHT=0; //UNDEFINED
    int TEMPSOIL=0; //UNDEFINED
    int MOIST=0; //UNDEFINED
    int WaterTank=0; //UNDEFINED
    int DEPTH_RX=0; //UNDEFINED
    int DEPTH_TX=0; //UNDEFINED
    int PH=0; //UNDEFINED
    int BATTERY_ADC=0; //UNDEFINED - mains-powered industrial relay module, no battery

    // PINOUT Relay - direct GPIO, confirmed identical across all three sources above.
    int RELAY_1=1;
    int RELAY_2=2;
    int RELAY_3=41;
    int RELAY_4=42;
    int RELAY_5=45;
    int RELAY_6=46;
    int RELAY_7=0; //UNDEFINED - only 6 relay channels on this kit
    int RELAY_8=0; //UNDEFINED
#else
    // ---- Classic ESP32 (esp32dev) / ESP32-S3 (esp32s3usbotg) - generic reference wiring ----

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
    // Roadmap #12: VoltageDivider battery sensing (ADC1_CH0, input-only, no other ConfigPin use)
    // - MAX17048 needs no dedicated pin, it shares the existing I2C bus (Wire.begin(), same as
    // BMP180/BMP280/BME280/CCS811) at its fixed address 0x36, which does not collide with any of
    // those (0x76/0x77/0x5A) or BH1750's default (0x23).
    int BATTERY_ADC=36;

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

    // Roadmap #149: nonzero = RELAY_1..8 above are PCF8574 bit indices on this I2C expander
    // (RelayIO.h routes pinMode/digitalWrite/digitalRead through Wire instead), zero = direct GPIO
    // (every kit except KC868-A6 today). SDA/SCL only matter when the address is nonzero.
#if defined(AGRUMY_KIT_KC868_A6)
    int RELAY_I2C_ADDRESS=0x24;
    int RELAY_I2C_SDA=4;
    int RELAY_I2C_SCL=15;
#else
    int RELAY_I2C_ADDRESS=0;
    int RELAY_I2C_SDA=0;
    int RELAY_I2C_SCL=0;
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
    // Roadmap #12: VoltageDivider calibration - the ACTUAL resistors wired (ohms), not an
    // abstract preset ratio, so BatteryLogic::computeDividerBatteryVoltage can recover the real
    // battery voltage regardless of which pair the admin used. Only meaningful when
    // sensorBattery selects the VoltageDivider option (2001); ignored by MAX17048 (1009), whose
    // fuel-gauge IC reports percentage directly. Defaults are the "Standard 1:1" preset - the
    // server always sends its own stored value on every config sync, this is only the
    // pre-first-sync fallback (same rule as the hysteresis defaults below).
    double batteryDividerR1 = 100000.0;
    double batteryDividerR2 = 100000.0;
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

    // Roadmap #39/#115: a third relay-control mode alongside threshold and interval above - "be
    // on during any of these wall-clock windows on these days", independent of any sensor
    // reading. Evaluated by ActuatorController::scheduleRelayFunction (RelayLogic::
    // computeAnyScheduleState, OR'd across the *ScheduleCount active slots) against LOCAL time,
    // derived on-device from DeviceConfig.utcOffsetSeconds (no timezone database needed here -
    // see that field's comment). Each ScheduleWindow's daysOfWeek is a 7-bit mask matching C's
    // tm_wday (bit 0 = Sunday .. bit 6 = Saturday); start/duration are seconds since local
    // midnight - v1 does not support a window crossing midnight (enforced server-side,
    // DeviceApiController.ScheduleWindowError). *ScheduleCount == 0 means "no windows configured"
    // - the old disabled flag's semantics, not "windows that are all currently off".
    ScheduleWindow ventilationSchedule[MAX_SCHEDULE_SLOTS_PER_FUNCTION];
    int ventilationScheduleCount = 0;
    ScheduleWindow lightSchedule[MAX_SCHEDULE_SLOTS_PER_FUNCTION];
    int lightScheduleCount = 0;
    ScheduleWindow heatingSchedule[MAX_SCHEDULE_SLOTS_PER_FUNCTION];
    int heatingScheduleCount = 0;
    ScheduleWindow waterPumpSchedule[MAX_SCHEDULE_SLOTS_PER_FUNCTION];
    int waterPumpScheduleCount = 0;

    // Roadmap #36: WaterPump-only device-side hard safety limits, independent of whichever mode
    // (threshold/interval/schedule/future #58 PID) decided the pump should be on - see
    // RelayLogic::runTimeCeilingHit/cooldownActive for the math and ActuatorController::
    // applyWaterPumpSafetyLimits for how they're combined. 0 disables either one. Server-
    // configurable per device (same ServerConfig-default-plus-per-device-override pattern as the
    // hysteresis fields above), not hardcoded, so a fallback of 0 here (never enforced) is safe
    // until the first real config sync fills it in - a device is never worse off than pre-#36.
    int waterPumpMaxRunSeconds = 0;
    int waterPumpCooldownSeconds = 0;

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

    // Roadmap #34: separate from configVersion on purpose (a queued command must not force a full
    // config re-apply, and vice versa) - see ServiceController::processPendingCommand for how
    // pendingCommand is acked/executed/reported.
    int commandVersion = 0;
    PendingCommand pendingCommand;

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
    // Roadmap #131: expected SHA-256 (lowercase hex) of the .bin at firmwareUrl, from the catalog
    // row's manifest hash - "" when the server has no hash for that source (OtaController skips
    // verification rather than failing closed, same tolerance as a pre-#131 firmware/catalog entry).
    String firmwareSha256;

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
    String apiCommandAck = "/api/Device/Command/Ack"; // roadmap #34

    String apiSensorDataPost="/api/SensorData";
    String apiSensorDataGet="";

};

// Roadmap #98/#129: main.cpp, DeviceController.cpp and SensorController.cpp each used to declare
// their OWN separate deviceConfig/service/serviceEndpoint - manually (and at least once
// forgetfully, see roadmap #80) kept in sync via explicit copy assignments after every config
// reload. Single canonical instances instead: defined once in main.cpp, extern-declared here so
// every translation unit reads/writes the SAME object - a config update is visible everywhere the
// moment it's applied, nothing to re-copy.
extern DeviceConfig deviceConfig;
extern ServiceEndpoint serviceEndpoint;



#endif