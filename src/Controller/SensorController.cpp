#include <Arduino.h>
#include <ArduinoJson.h>
#include <SPI.h>

#include <Wire.h>
#include <DHT.h> // DHT temp, humidity
#include <DHT_U.h>
#include <BH1750.h> // Light sensor

#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085.h> // BMP180 temp, pressure
#include <Adafruit_BMP280.h>
#include <Adafruit_CCS811.h> // CCS811 CO2, TVOC
#include <SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library.h> // MAX17048 battery fuel gauge, roadmap #12

#include <esp_task_wdt.h>

#include "SensorController.h"
#include "DeviceController.h"
#include "ServiceController.h"
#include "ActuatorController.h"
#include "../Logic/BatteryLogic.h" // roadmap #12: divider math + LiPo voltage->percent curve

DeviceConfig deviceConfig;
ServiceEndpoint serviceEndpoint;

DeviceController device;
ServiceController service;
ActuatorController controller;

static JsonDocument jsonDoc;
static JsonArray sensorDataJsonArray = jsonDoc.to<JsonArray>();
static String dateTime;

static Adafruit_CCS811 ccs811;                               // Co2, Tvoc
static DHT_Unified dht11(deviceConfig.configPin.DHT, DHT11); // temp, humidity
static DHT_Unified dht22(deviceConfig.configPin.DHT, DHT22); // temp, humidity
static Adafruit_BMP085 bmp180;                               // temp, pressure
static Adafruit_BMP280 bmp280;                               // temp, pressure
BH1750 Bh1750;                                               // light
static SFE_MAX1704X maxlipo;                                  // battery fuel gauge, roadmap #12

static unsigned bmp280status;
static unsigned bmp180status;
static unsigned bh1750status;
static bool max17048status;


void SensorController::setupSensor()
{
    Serial.println("[Sensor setup]");

    dht11.begin();
    dht22.begin();

    bmp180status = bmp180.begin(0x77);
    bmp280status = bmp280.begin(0x76);

    // CO2, TVOC
    ccs811.begin(0x5A); // 0x5B is default, mine is older version

    // Light
    bh1750status = Bh1750.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);

    // Battery fuel gauge (roadmap #12) - fixed I2C address 0x36, shares the bus already begun
    // above for BMP180/BMP280/CCS811. Harmless to call begin() when BatterySensorType is None or
    // VoltageDivider - it just never gets read in buildSensorData()'s switch either way.
    max17048status = maxlipo.begin();

    delay(5000);
}

// Temperature and humidity sensors

void SensorController::sensor_DHT11_temp()
{

    Serial.println("[Sensor] DHT11 temperature");
    sensors_event_t event;

    dht11.temperature().getEvent(&event);
    if (isnan(event.temperature))
    {
        Serial.println("Error reading temperature!");
    }
    else
    {
        Serial.print("Temperature = ");
        Serial.print(event.temperature);
        Serial.println(" C");
        sensorData.temperature=event.temperature;
        
    }
}
void SensorController::sensor_DHT11_humid()
{

    Serial.println("[Sensor] DHT11 humidity");
    sensors_event_t event;

    dht11.humidity().getEvent(&event);
    if (isnan(event.relative_humidity))
    {
        Serial.println("Error reading humidity!");
    }
    else
    {
        Serial.print("Humidity = ");
        Serial.print(event.relative_humidity);
        Serial.println(" %");
        sensorData.humidity=event.relative_humidity;
    }
}

void SensorController::sensor_DHT22_temp()
{

    Serial.println("[Sensor] DHT22 temperature");
    delay(500);
    dht22.begin();
    delay(500);
    sensors_event_t event;

    dht22.temperature().getEvent(&event);
    if (isnan(event.temperature))
    {
        Serial.println("Error reading temperature!");
    }
    else
    {
        Serial.print("Temperature = ");
        Serial.print(event.temperature);
        Serial.println(" %");
        sensorData.temperature=event.temperature;
    }
}
void SensorController::sensor_DHT22_humid()
{

    Serial.println("[Sensor] DHT22 humidity");
    delay(500);
    dht22.begin();
    delay(500);
    sensors_event_t event;

    dht22.humidity().getEvent(&event);
    if (isnan(event.relative_humidity))
    {
        Serial.println("Error reading humidity!");
    }
    else
    {
        Serial.print("Humidity = ");
        Serial.print(event.relative_humidity);
        Serial.println(" %");
        sensorData.humidity=event.relative_humidity;
    }
}

void SensorController::sensor_BMP180_temp()
{
    Serial.println("[Sensor] BMP180 temperature");
    if (!bmp180status)
    {
        Serial.println("[Sensor] BMP180 error reading data");
        sensorData.eventlog.error = true;
    }
    else
    {

        double temperatureC = bmp180.readTemperature();

        Serial.print("Temperature = ");
        Serial.print(temperatureC);
        Serial.println(" *C");
        sensorData.temperature=temperatureC;
    }
}
void SensorController::sensor_BMP180_pres()
{
    Serial.println("[Sensor] BMP180 pressure");
    if (!bmp180status)
    {
        Serial.println("[Sensor] BMP180 error reading data");
        sensorData.eventlog.error = true;
    }
    else
    {

        double barometer = bmp180.readPressure();

        Serial.print("Pressure = ");
        Serial.print(barometer);
        Serial.println(" Pa");
        sensorData.barometer=barometer;
    }
}

void SensorController::sensor_BMP280_temp()
{
    Serial.println("[Sensor] BMP280 temperature");
    if (!bmp280status)
    {
        Serial.println("[Sensor] BMP280 error reading data");
        sensorData.eventlog.error = true;
    }
    else
    {

        double temperatureC = bmp280.readTemperature();

        Serial.print("Temperature = ");
        Serial.print(temperatureC);
        Serial.println(" *C");
        sensorData.temperature=temperatureC;
    }
}
void SensorController::sensor_BMP280_pres()
{
    Serial.println("[Sensor] BMP280 pressure");
    if (!bmp280status)
    {
        Serial.println("[Sensor] BMP280 error reading data");
        sensorData.eventlog.error = true;
    }
    else
    {

        double barometer = bmp280.readPressure();

        Serial.print("Pressure = ");
        Serial.print(barometer);
        Serial.println(" Pa");
        sensorData.barometer=barometer;
    }
}

void SensorController::sensor_BME280_temp()
{
}
void SensorController::sensor_BME280_humid()
{
}
void SensorController::sensor_BME280_pres()
{
}

// Soil temperature sensors
void SensorController::sensor_DS18B20_temp()
{
}

// Light sensors
void SensorController::sensor_BH1750_lux()
{
    Serial.println("[Sensor] BH1750 lux");
    BH1750 lightMeter;

    lightMeter.begin();
    delay(500);

    double lux = lightMeter.readLightLevel();
    Serial.print("Light: ");
    Serial.print(lux);
    Serial.println(" lx");
    Serial.println();

    sensorData.light = lux;
}

// Co2 and Tvoc sensors
void SensorController::sensor_CCS811_co2()
{
    // CO2 and TVOC sensor, needs time (~20min) to heat up and give info
    EventLog eventlog;
    Serial.println("[Sensor] CCS811 Co2");

    if (ccs811.available())
    {
        if (!ccs811.readData())
        {
            Serial.print("CO2: ");
            Serial.println(ccs811.geteCO2());
            sensorData.co2 = ccs811.geteCO2();
        }
        else
        {
            eventlog.error = true;
            eventlog.errorCode = 2106;
            eventlog.errorData = "Waiting for sensor to heat up, no data";
        }
    }

}
void SensorController::sensor_CCS811_tvoc()
{
    // CO2 and TVOC sensor, needs time (~20min) to heat up and give info
    EventLog eventlog;
    Serial.println("[Sensor] CCS811 Tvoc");

    if (ccs811.available())
    {
        if (!ccs811.readData())
        {
            Serial.print("ppm, TVOC: ");
            Serial.println(ccs811.getTVOC());
            sensorData.tvoc = ccs811.getTVOC();
        }
        else
        {
            eventlog.error = true;
            eventlog.errorCode = 2106;
            eventlog.errorData = "Waiting for sensor to heat up, no data";
        }
    }

}

void SensorController::sensor_analog_voltage() // 2001 - roadmap #12 VoltageDivider
{
    Serial.println("[Sensor battery - voltage divider]");
    device.powerRailSecondary(true);

    // analogReadMilliVolts(), not raw analogRead() + a hand-rolled mV conversion - the ESP32
    // Arduino core already runs the reading through esp-idf's adc_cali API (eFuse-based
    // per-chip calibration), which is the automatic-calibration path roadmap #12 deliberately
    // relies on instead of a manual calibration pass.
    uint32_t measuredMilliVolts = analogReadMilliVolts(device.deviceConfig.configPin.BATTERY_ADC);
    double batteryVoltage = computeDividerBatteryVoltage(
        measuredMilliVolts / 1000.0,
        deviceConfig.configSensor.batteryDividerR1,
        deviceConfig.configSensor.batteryDividerR2);
    int percent = computeBatteryPercentFromVoltage(batteryVoltage);

    device.powerRailSecondary(false);

    Serial.print("Battery voltage: ");
    Serial.print(batteryVoltage);
    Serial.print("V (");
    Serial.print(percent);
    Serial.println("%)");

    sensorData.battery = String(percent);
}

void SensorController::sensor_battery_max17048() // 1009 - roadmap #12, RECOMMENDED option
{
    Serial.println("[Sensor battery - MAX17048]");
    if (!max17048status)
    {
        Serial.println("MAX17048 not detected on I2C bus - skipping battery reading");
        return;
    }

    // getSOC() (state of charge) is the fuel gauge's own coulomb-counting percentage - no
    // voltage curve needed here, that is exactly the precision advantage over VoltageDivider.
    float percent = maxlipo.getSOC();
    sensorData.battery = String((int)constrain(percent, 0.0f, 100.0f));
}

void SensorController::sensor_analog_moist()
{
    
    Serial.println("[Sensor moisture]");
    device.powerRailSecondary(true);
    
    int moisture = analogRead(device.deviceConfig.configPin.MOIST);

    Serial.print("Analog: ");
    Serial.println(moisture);

    int soilWet = 1200; // Define max value we consider soil 'wet'
    int soilDry = 3000;
    if (moisture < soilWet)
    {
        Serial.println("Status: Soil is too wet");
    }
    else if (moisture >= soilWet && moisture < soilDry)
    {
        Serial.println("Status: Soil is moist");
    }
    else
    {
        Serial.println("Status: Soil is too dry");
    }

    // TODO: convert raw reading to a 0-100 percentage (100/4096 per step)

    if (moisture != 0)
    {
        sensorData.moisture = moisture;
    }
    else
    {
        Serial.println("Moisture sensor not present");
    }

    device.powerRailSecondary(false);
    Serial.println();
    // TODO: eventlog error when reading is 0 (no sensor present)
}
void SensorController::sensor_Wind()
{
}

void SensorController::sensor_liquid_PH()
{
}

void SensorController::sensor_analog_waterLevel()
{
    Serial.println("[Sensor water level]");
    device.powerRailSecondary(true);

    int waterTank = analogRead(device.deviceConfig.configPin.WaterTank);

    Serial.print("Analog: ");
    Serial.println(waterTank);

    sensorData.waterLevel = waterTank;

    device.powerRailSecondary(false);
}

void SensorController::sensor_rainLevel()
{
}

void SensorController::buildSensorDataPayload()
{
    Serial.println(sensorData.temperature);
    Serial.println(sensorData.humidity);
    Serial.println(sensorData.barometer);
    Serial.println(sensorData.co2);
    Serial.println(sensorData.tvoc);
    Serial.println(sensorData.light);

    JsonDocument jsonSensorData;

    jsonSensorData["deviceID"]=deviceConfig.deviceID;
    jsonSensorData["tenantID"]=deviceConfig.tenantID;
    jsonSensorData["deviceUnitID"]=deviceConfig.deviceUnitID;
    jsonSensorData["deviceUnitZoneID"]=deviceConfig.deviceUnitZoneID;


    jsonSensorData["temperature"]=(sensorData.temperature)!=""? sensorData.temperature:  JsonVariant(); // JsonVariant() for null; (char*)0 was memory-unsafe
    jsonSensorData["soilTemperature"]=(sensorData.temperatureSoil)!=""? sensorData.temperatureSoil:  JsonVariant();
    jsonSensorData["humidity"]=(sensorData.humidity)!=""? sensorData.humidity:  JsonVariant();
    jsonSensorData["battery"]=(sensorData.battery)!=""? sensorData.battery:  JsonVariant();
    jsonSensorData["moisture"]=(sensorData.moisture)!=""? sensorData.moisture:  JsonVariant();
    jsonSensorData["light"]=(sensorData.light)!=""? sensorData.light:  JsonVariant();
    jsonSensorData["co2"]=(sensorData.co2)!=""? sensorData.co2:  JsonVariant();
    jsonSensorData["tvoc"]=(sensorData.tvoc)!=""? sensorData.tvoc:  JsonVariant();
    jsonSensorData["barometer"]=(sensorData.barometer)!=""? sensorData.barometer:  JsonVariant();
    jsonSensorData["liquidPH"]=(sensorData.liquidPH)!=""? sensorData.liquidPH:  JsonVariant();
    jsonSensorData["rainLevel"]=(sensorData.rainLevel)!=""? sensorData.rainLevel:  JsonVariant();
    jsonSensorData["waterLevel"]=(sensorData.waterLevel)!=""? sensorData.waterLevel:  JsonVariant();
    jsonSensorData["wind"]=(sensorData.wind)!=""? sensorData.wind:  JsonVariant();
    jsonSensorData["dateCreated"]=(device.getDateTime())!=""? device.getDateTime():  JsonVariant(); // timestamp for buffering

    sensorDataJsonArray.add(jsonSensorData); // buffer if the service point is unavailable

    String sensorDataDebug;
    serializeJsonPretty(jsonSensorData,sensorDataDebug);

    Serial.println("[Sensor] Buffered sensorData:");
    Serial.println(sensorDataDebug);

    pushSensorData(sensorDataJsonArray); 
}

// Roadmap #9: one full RAM buffer's worth per file. A single reading is ~400 bytes, so this
// spills roughly every 20 failed cycles; ~170 files fit under the 70% partition cap.
static const size_t SENSOR_BUFFER_SPILL_BYTES = 8192;

bool SensorController::flushBufferedSensorData()
{
    String filename = device.oldestBufferedSensorFile();
    if (filename.isEmpty())
    {
        return true; // nothing queued - the common case, one directory scan and out
    }

    serviceRequest.endpoint = serviceEndpoint.apiSensorDataPost;
    serviceRequest.header.apiId = deviceConfig.apiId;

    while (!filename.isEmpty())
    {
        String payloadJson = device.loadFile(filename);

        JsonDocument payload;
        if (payloadJson.isEmpty() || deserializeJson(payload, payloadJson) != DeserializationError::Ok)
        {
            // A poison entry (corruption the #62-style atomic write can't rule out once the file
            // is already committed) would wedge the whole queue forever - drop it, keep draining.
            Serial.println("[Sensor] Buffered file /" + filename + " unreadable - dropping it");
            device.removeBufferedFile(filename);
        }
        else if (service.requestPost(payload, serviceRequest).eventlog.error)
        {
            Serial.println("[Sensor] Flush stopped at /" + filename + " - connection lost again, remaining files stay queued");
            return false;
        }
        else
        {
            Serial.println("[Sensor] Flushed /" + filename);
            device.removeBufferedFile(filename); // per-file delete, only after ITS OWN 2xx
        }

        // Each file is a full HTTP round-trip (TLS handshake included) - a deep backlog would
        // outlast the 90 s task WDT without feeding it per file. Same call main.cpp's
        // sleep-chunking already uses.
        esp_task_wdt_reset();

        filename = device.oldestBufferedSensorFile();
    }
    return true;
}

void SensorController::pushSensorData(JsonDocument payload){

    // Roadmap #80 (same bug class as #77): the file-static `service` above is a SEPARATE
    // ServiceController instance from main.cpp's - its deviceConfig was never assigned, so
    // requestPost()'s servicePublicKey.length()>0 check was always false and sensor-data push
    // silently ignored an operator-pinned self-hosted cert, always falling back to the public CA
    // bundle. One assignment here covers every service.requestPost()/pushEvent() call this
    // function reaches (flushBufferedSensorData() included - it has no other caller).
    service.deviceConfig = deviceConfig;

    serviceRequest.endpoint = serviceEndpoint.apiSensorDataPost;
    serviceRequest.header.apiId = deviceConfig.apiId;

    // Roadmap #9: the disk backlog goes first, oldest file first, so the server receives rows in
    // chronological order; the live RAM payload is only attempted once the backlog fully drained.
    // A flush that broke off means the connection is down again - skip the doomed live attempt,
    // the readings just keep accumulating below.
    bool sent = false;
    if (flushBufferedSensorData())
    {
        sent = !service.requestPost(payload, serviceRequest).eventlog.error; // 2xx - requestPost marks 200/201 as success
    }

    if (sent)
    {
        Serial.println("[Sensor] SensorData uploaded, resetting sensorData buffer");
        sensorDataJsonArray = jsonDoc.to<JsonArray>();
        return;
    }

    // Failed send: readings stay in the RAM array (pre-#9 behaviour) but now with a cap - at
    // SENSOR_BUFFER_SPILL_BYTES the array spills to one /buffer file and RAM restarts empty,
    // instead of the old unbounded growth until the heap died.
    size_t pending = measureJson(sensorDataJsonArray);
    Serial.printf("[Sensor] SensorData send failed - %u bytes pending in RAM buffer\n", (unsigned)pending);
    if (pending >= SENSOR_BUFFER_SPILL_BYTES)
    {
        String spill;
        serializeJson(sensorDataJsonArray, spill);
        if (!device.bufferSensorDataToDisk(spill))
        {
            // Partition >= 70% full: deliberate data loss by design. Fire-and-forget #28 event -
            // unreachable while fully offline (chicken-and-egg, same as NoInternet), but a
            // server-side outage with intact connectivity WILL land it.
            service.pushEvent(serviceRequest, "BufferDiscarded", "LittleFS >= 70% full, dropped " + String((unsigned)pending) + " bytes of sensor data");
        }
        sensorDataJsonArray = jsonDoc.to<JsonArray>();
    }
}

void SensorController::buildSensorData(DeviceConfig deviceConfig)
{
    sensorData.battery="";
    sensorData.temperature="";
    sensorData.temperatureSoil="";
    sensorData.humidity="";
    sensorData.moisture="";
    sensorData.light="";
    sensorData.co2="";
    sensorData.tvoc="";
    sensorData.barometer="";
    sensorData.liquidPH="";
    sensorData.rainLevel="";
    sensorData.waterLevel="";
    sensorData.wind="";

    // Battery - roadmap #12
    switch (deviceConfig.configSensor.sensorBattery)
    {
    case 1009:
        sensor_battery_max17048();
        break;
    case 2001:
        sensor_analog_voltage();
        break;
    default:
        break;
    }

    // Temperature
    switch (deviceConfig.configSensor.sensorTemp)
    {
    case 1001:
        sensor_DHT11_temp();
        break;
    case 1002:
        sensor_DHT22_temp();
        break;
    case 1003:
        sensor_BMP180_temp();
        break;
    case 1004:
        sensor_BMP280_temp();
        break;
    case 1005:
        sensor_BME280_temp();
        break;
    default:
        break;
    }

    // Temperature soil
    switch (deviceConfig.configSensor.sensorTempSoil)
    {
    case 1007:
        sensor_DS18B20_temp();
        break;

    default:
        break;
    }

    // Humidity
    switch (deviceConfig.configSensor.sensorHumid)
    {
    case 1001:
        sensor_DHT11_humid();
        break;

    default:
        break;
    }
    // Humidity
    switch (deviceConfig.configSensor.sensorHumid)
    {
    case 1002:
        sensor_DHT22_humid();
        break;

    default:
        break;
    }
    // Humidity
    switch (deviceConfig.configSensor.sensorHumid)
    {
    case 1005:
        sensor_BME280_humid();
        break;

    default:
        break;
    }

    // Moisture
    switch (deviceConfig.configSensor.sensorMoist)
    {
    case 2002:
        sensor_analog_moist();
        break;

    default:
        break;
    }

    // Lux
    switch (deviceConfig.configSensor.sensorLight)
    {
    case 1008:
        sensor_BH1750_lux();
        break;

    default:
        break;
    }

    // Co2
    switch (deviceConfig.configSensor.sensorCo2)
    {
    case 1006:
        sensor_CCS811_co2();
        break;

    default:
        break;
    }

    // Tvoc
    switch (deviceConfig.configSensor.sensorTvoc)
    {
    case 1006:
        sensor_CCS811_tvoc();
        break;

    default:
        break;
    }

    // Pressure
    switch (deviceConfig.configSensor.sensorBarometer)
    {
    case 1003:
        sensor_BMP180_pres();
        break;

    default:
        break;
    }
    // Pressure
    switch (deviceConfig.configSensor.sensorBarometer)
    {
    case 1004:
        sensor_BMP280_pres();
        break;

    default:
        break;
    }
    // Pressure
    switch (deviceConfig.configSensor.sensorBarometer)
    {
    case 1005:
        sensor_BME280_pres();
        break;

    default:
        break;
    }

    // Water PH
    switch (deviceConfig.configSensor.sensorPH)
    {
    case 0:
        sensor_liquid_PH();
        break;

    default:
        break;
    }

    // Water tank
    switch (deviceConfig.configSensor.sensorWaterLevel)
    {
    case 2003:
        sensor_analog_waterLevel();
        break;

    default:
        break;
    }

    // Rain
    switch (deviceConfig.configSensor.sensorRainLevel)
    {
    case 0:
        sensor_rainLevel();
        break;

    default:
        break;
    }

    // Wind
    switch (deviceConfig.configSensor.sensorWind)
    {
    case 0:
        sensor_Wind();
        break;

    default:
        break;
    }

    buildSensorDataPayload();

    if(deviceConfig.deviceControllerEnabled){
        // The instance acting on relays is THIS file's `controller` global - main.cpp's is a
        // separate `static` object that received deviceConfig but was never asked to run.
        // Without this hand-off the acting instance keeps a zero ConfigController (relay
        // assignments, thresholds, intervals), so every switch in initController() hits
        // "case 0" and the whole control path is a silent no-op on real hardware.
        controller.deviceConfig = deviceConfig;
        controller.initController(sensorData, device.getEpochSeconds());
    }

}