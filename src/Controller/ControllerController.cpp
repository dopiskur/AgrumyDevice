#include "arduino.h"
#include <WiFi.h> //wifi connection WiFi.begin(ssid, password)
#include <EEPROM.h>
#include "SPIFFS.h"
#include "FS.h"
#include "WiFiManager.h"

#include "DeviceController.h"
#include "ServiceController.h"
#include "ControllerController.h"

static DeviceDefaults deviceDefaults;
static DeviceConfig deviceConfig;
static ServiceEndpoint serviceEndpoint;

void ControllerController::initController(SensorData sensorData)
{
    switch (deviceConfig.configController.relay1)
    {
    case 0:
        break;
    case 1:
        relayLight(deviceConfig.configController.relay1, sensorData);
        break;

    case 2:
        relayWaterPump(deviceConfig.configController.relay1, sensorData);
        break;

    case 3:
        relayHeating(deviceConfig.configController.relay1, sensorData);
        break;

    case 4:
        relayVentilation(deviceConfig.configController.relay1, sensorData);
        break;

    default:
        break;
    }

        switch (deviceConfig.configController.relay2)
    {
    case 0:
        break;
    case 1:
        relayLight(deviceConfig.configController.relay2, sensorData);
        break;

    case 2:
        relayWaterPump(deviceConfig.configController.relay2, sensorData);
        break;

    case 3:
        relayHeating(deviceConfig.configController.relay2, sensorData);
        break;

    case 4:
        relayVentilation(deviceConfig.configController.relay2, sensorData);
        break;

    default:
        break;
    }

        switch (deviceConfig.configController.relay3)
    {
    case 0:
        break;
    case 1:
        relayLight(deviceConfig.configController.relay3, sensorData);
        break;

    case 2:
        relayWaterPump(deviceConfig.configController.relay3, sensorData);
        break;

    case 3:
        relayHeating(deviceConfig.configController.relay3, sensorData);
        break;

    case 4:
        relayVentilation(deviceConfig.configController.relay3, sensorData);
        break;

    default:
        break;
    }

        switch (deviceConfig.configController.relay4)
    {
    case 0:
        break;
    case 1:
        relayLight(deviceConfig.configController.relay4, sensorData);
        break;

    case 2:
        relayWaterPump(deviceConfig.configController.relay4, sensorData);
        break;

    case 3:
        relayHeating(deviceConfig.configController.relay4, sensorData);
        break;

    case 4:
        relayVentilation(deviceConfig.configController.relay4, sensorData);
        break;

    default:
        break;
    }

        switch (deviceConfig.configController.relay5)
    {
    case 0:
        break;
    case 1:
        relayLight(deviceConfig.configController.relay5, sensorData);
        break;

    case 2:
        relayWaterPump(deviceConfig.configController.relay5, sensorData);
        break;

    case 3:
        relayHeating(deviceConfig.configController.relay5, sensorData);
        break;

    case 4:
        relayVentilation(deviceConfig.configController.relay5, sensorData);
        break;

    default:
        break;
    }

        switch (deviceConfig.configController.relay6)
    {
    case 0:
        break;
    case 1:
        relayLight(deviceConfig.configController.relay6, sensorData);
        break;

    case 2:
        relayWaterPump(deviceConfig.configController.relay6, sensorData);
        break;

    case 3:
        relayHeating(deviceConfig.configController.relay6, sensorData);
        break;

    case 4:
        relayVentilation(deviceConfig.configController.relay6, sensorData);
        break;

    default:
        break;
    }

        switch (deviceConfig.configController.relay7)
    {
    case 0:
        break;
    case 1:
        relayLight(deviceConfig.configController.relay7, sensorData);
        break;

    case 2:
        relayWaterPump(deviceConfig.configController.relay7, sensorData);
        break;

    case 3:
        relayHeating(deviceConfig.configController.relay7, sensorData);
        break;

    case 4:
        relayVentilation(deviceConfig.configController.relay7, sensorData);
        break;

    default:
        break;
    }

        switch (deviceConfig.configController.relay8)
    {
    case 0:
        break;
    case 1:
        relayLight(deviceConfig.configController.relay8, sensorData);
        break;

    case 2:
        relayWaterPump(deviceConfig.configController.relay8, sensorData);
        break;

    case 3:
        relayHeating(deviceConfig.configController.relay8, sensorData);
        break;

    case 4:
        relayVentilation(deviceConfig.configController.relay8, sensorData);
        break;

    default:
        break;
    }
}

void ControllerController::relayVentilation(int relayPin, SensorData sensorData)
{
    const int powerPin = relayPin;
    pinMode(powerPin, OUTPUT);

    double humidHigh = atof(sensorData.humidity.c_str());

    if (humidHigh > deviceConfig.configController.humidHigh)
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
};

void ControllerController::relayWaterPump(int relayPin, SensorData sensorData)
{
    const int powerPin = relayPin;
    pinMode(powerPin, OUTPUT);

    double waterLevel = atof(sensorData.waterLevel.c_str());

    if (waterLevel < deviceConfig.configController.waterLow)
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
};

void ControllerController::relayHeating(int relayPin, SensorData sensorData)
{
    const int powerPin = relayPin;
    pinMode(powerPin, OUTPUT);

    double temperature = atof(sensorData.temperature.c_str());

    if (temperature < deviceConfig.configController.tempLow)
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
};

void ControllerController::relayLight(int relayPin, SensorData sensorData)
{
    const int powerPin = relayPin;
    pinMode(powerPin, OUTPUT);

    double lightLow = atof(sensorData.light.c_str());

    if (lightLow < deviceConfig.configController.lightLow)
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
};