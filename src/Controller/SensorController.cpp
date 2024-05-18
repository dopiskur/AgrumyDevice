#include <Arduino.h>
#include <ArduinoJson.h>
#include <SPI.h>

// Sensor libraries
#include <Wire.h>
#include <DHT.h> // DHT temp, humidity
#include <DHT_U.h>
#include <BH1750.h> // Light sensor

#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085.h> // BMP180 temp, pressure
#include <Adafruit_BMP280.h>
#include <Adafruit_CCS811.h> // CCS811 CO2, TVOC

#include "SensorController.h"
#include "ControllerController.h"

// Models
DeviceConfig deviceConfig;
ServiceEndpoint serviceEndpoint;

// Controllers
DeviceController device;
ServiceController service;
ControllerController controller;

// static vars
static JsonDocument jsonDoc;
static JsonArray sensorDataJsonArray = jsonDoc.to<JsonArray>(); // prepare array with structure
static String dateTime;

// Sensors
static Adafruit_CCS811 ccs811;                               // Co2, Tvoc
static DHT_Unified dht11(deviceConfig.configPin.DHT, DHT11); // temp, humidity
static DHT_Unified dht22(deviceConfig.configPin.DHT, DHT22); // temp, humidity
static Adafruit_BMP085 bmp180;                               // temp, pressure
static Adafruit_BMP280 bmp280;                               // temp, pressure
BH1750 Bh1750;                                               // light

static unsigned bmp280status;
static unsigned bmp180status;
static unsigned bh1750status;


void SensorController::setupSensor()
{
    Serial.println("[Sensor setup]");
    // early initalization of sensors
    // DHT 11 temp, humid

    dht11.begin();
    dht22.begin();

    bmp180status = bmp180.begin(0x77);
    bmp280status = bmp280.begin(0x76);

    // CO2, TVOC
    ccs811.begin(0x5A); // 0x5B is default, mine is older version

    // Light
    bh1750status = Bh1750.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);

    delay(5000);
}

//// Read sensor data

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
EventLog SensorController::sensor_CCS811_co2()
{
    // CO2 and TVOC sensor, needs time (~20min) to heat up and give info
    EventLog eventlog;
    Serial.println("[Sensor] CCS811 Co2");
    // Adafruit_CCS811 ccs;
    // ccs.begin(0x5A); // 0x5B is default

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

    return eventlog;
}
EventLog SensorController::sensor_CCS811_tvoc()
{
    // CO2 and TVOC sensor, needs time (~20min) to heat up and give info
    EventLog eventlog;
    Serial.println("[Sensor] CCS811 Tvoc");
    // Adafruit_CCS811 ccs;
    // ccs.begin(0x5A); // 0x5B is default

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

    return eventlog;
}

void SensorController::sensor_analog_voltage()
{
    device.powerRailSecondary(true);
    device.powerRailSecondary(false);
}
void SensorController::sensor_analog_moist()
{
    
    Serial.println("[Sensor moisture]");
    device.powerRailSecondary(true);
    
    int moisture = analogRead(device.deviceConfig.configPin.MOIST);

    Serial.print("Analog: ");
    Serial.println(moisture); // analog value

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

    // implementiraj racunanje u postotak!!@!
    //  create % value, 100/4096=0,0244140625
    // moisture = 100-moisture; //create to scale from 0 to 100;

    if (!moisture == 0)
    {
        sensorData.moisture = moisture;
    }
    else
    {
        Serial.println("Moisture sensor not present");
    }

    device.powerRailSecondary(false);
    Serial.println();
    // if 0 evenlog error, no sensor present!
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
    Serial.println(waterTank); // analog value

    sensorData.moisture = waterTank;
    
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


    jsonSensorData["temperature"]=(sensorData.temperature)!=""? sensorData.temperature:  (char*)0;
    jsonSensorData["soilTemperature"]=(sensorData.temperatureSoil)!=""? sensorData.temperatureSoil:  (char*)0;
    jsonSensorData["humidity"]=(sensorData.humidity)!=""? sensorData.humidity:  (char*)0;
    jsonSensorData["battery"]=(sensorData.battery)!=""? sensorData.battery:  (char*)0;
    jsonSensorData["moisture"]=(sensorData.moisture)!=""? sensorData.moisture:  (char*)0;
    jsonSensorData["light"]=(sensorData.light)!=""? sensorData.light:  (char*)0;
    jsonSensorData["co2"]=(sensorData.co2)!=""? sensorData.co2:  (char*)0;
    jsonSensorData["tvoc"]=(sensorData.tvoc)!=""? sensorData.tvoc:  (char*)0;
    jsonSensorData["barometer"]=(sensorData.barometer)!=""? sensorData.barometer:  (char*)0;
    jsonSensorData["liquidPH"]=(sensorData.liquidPH)!=""? sensorData.liquidPH:  (char*)0;
    jsonSensorData["rainLevel"]=(sensorData.rainLevel)!=""? sensorData.rainLevel:  (char*)0;
    jsonSensorData["waterLevel"]=(sensorData.waterLevel)!=""? sensorData.waterLevel:  (char*)0;
    jsonSensorData["wind"]=(sensorData.wind)!=""? sensorData.wind:  (char*)0;
    jsonSensorData["dateCreated"]=(device.getDateTime())!=""? device.getDateTime():  (char*)0; // adding datetime stamp for buffering purposes

    sensorDataJsonArray.add(jsonSensorData); // add document to array, buffering data if servicepoint not available

    String sensorDataDebug;
    serializeJsonPretty(jsonSensorData,sensorDataDebug);

    Serial.println("[Sensor] Buffered sensorData:");
    Serial.println(sensorDataDebug);

    pushSensorData(sensorDataJsonArray); 
}

void SensorController::pushSensorData(JsonDocument payload){

    serviceRequest.endpoint = serviceEndpoint.apiSensorDataPost;
    serviceRequest.header.apiId = deviceConfig.apiId;
    //prvo to treba prevorit u array;
    ServiceData serviceData = service.requestPost(payload,serviceRequest);
    if(serviceData.eventlog.errorCode==200){
        Serial.println("[Sensor] SensorData uploadad, reseting sensorData buffer");
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

    // Battery

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

    // start controller
    if(deviceConfig.deviceControllerEnabled==true){
        controller.initController(sensorData);
    }

}