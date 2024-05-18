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

// Milis vars
unsigned long millisCurrent;
unsigned long millisStartVentilation;
unsigned long millisStartVentilationLenght;
unsigned long millisStartLight;
unsigned long millisStartLightLenght;
unsigned long millisStartHeating;
unsigned long millisStartHeatingLenght;
unsigned long millisStartWaterPump;
unsigned long millisStartWaterPumpLenght;


void ControllerController::setupController(){


}

void ControllerController::intervalVentilation()
{
    millisCurrent = millis();   

    // wait time before the loop starts again
    if (deviceConfig.configController.ventilationIntervalEnabled == true)
    {
        if(!(millisCurrent - millisStartVentilation >= deviceConfig.configController.ventilationInterval*1000)){
            return;
        }
    
        int relay1=0;
        int relay2=0;
        int relay3=0;
        int relay4=0;
        int relay5=0;
        int relay6=0;
        int relay7=0;
        int relay8=0;

        if(deviceConfig.configController.relay1==1) { relay1 = deviceConfig.configPin.RELAY_1; } ;// 1 is value of ventilation
        if(deviceConfig.configController.relay2==1) { relay1 = deviceConfig.configPin.RELAY_2; } ;
        if(deviceConfig.configController.relay3==1) { relay1 = deviceConfig.configPin.RELAY_3; } ;
        if(deviceConfig.configController.relay4==1) { relay1 = deviceConfig.configPin.RELAY_4; } ;
        if(deviceConfig.configController.relay5==1) { relay1 = deviceConfig.configPin.RELAY_5; } ;
        if(deviceConfig.configController.relay6==1) { relay1 = deviceConfig.configPin.RELAY_6; } ;
        if(deviceConfig.configController.relay7==1) { relay1 = deviceConfig.configPin.RELAY_7; } ;
        if(deviceConfig.configController.relay8==1) { relay1 = deviceConfig.configPin.RELAY_8; } ;

        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay1, HIGH);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay2, HIGH);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay3, HIGH);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay4, HIGH);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay5, HIGH);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay6, HIGH);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay7, HIGH);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay8, HIGH);};
    
    if(!(millisCurrent - millisStartVentilationLenght >= deviceConfig.configController.ventilationInterval*1000)){
            return;
        }

        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay1, LOW);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay2, LOW);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay3, LOW);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay4, LOW);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay5, LOW);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay6, LOW);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay7, LOW);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay8, LOW);};    

    }


}

void ControllerController::intervalLight()
{
millisCurrent = millis();   

    // wait time before the loop starts again
    if (deviceConfig.configController.lightIntervalEnabled == true)
    {
        if(!(millisCurrent - millisStartLight >= deviceConfig.configController.lightInterval*1000)){
            return;
        }
    
        int relay1=0;
        int relay2=0;
        int relay3=0;
        int relay4=0;
        int relay5=0;
        int relay6=0;
        int relay7=0;
        int relay8=0;

        if(deviceConfig.configController.relay1==1) { relay1 = deviceConfig.configPin.RELAY_1; } ;// 1 is value of ventilation
        if(deviceConfig.configController.relay2==1) { relay1 = deviceConfig.configPin.RELAY_2; } ;
        if(deviceConfig.configController.relay3==1) { relay1 = deviceConfig.configPin.RELAY_3; } ;
        if(deviceConfig.configController.relay4==1) { relay1 = deviceConfig.configPin.RELAY_4; } ;
        if(deviceConfig.configController.relay5==1) { relay1 = deviceConfig.configPin.RELAY_5; } ;
        if(deviceConfig.configController.relay6==1) { relay1 = deviceConfig.configPin.RELAY_6; } ;
        if(deviceConfig.configController.relay7==1) { relay1 = deviceConfig.configPin.RELAY_7; } ;
        if(deviceConfig.configController.relay8==1) { relay1 = deviceConfig.configPin.RELAY_8; } ;

        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay1, HIGH);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay2, HIGH);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay3, HIGH);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay4, HIGH);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay5, HIGH);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay6, HIGH);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay7, HIGH);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay8, HIGH);};
    
    if(!(millisCurrent - millisStartLightLenght >= deviceConfig.configController.lightInterval*1000)){
            return;
        }

        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay1, LOW);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay2, LOW);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay3, LOW);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay4, LOW);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay5, LOW);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay6, LOW);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay7, LOW);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay8, LOW);};    

    }

}

void ControllerController::intervalHeating()
{
millisCurrent = millis();   

    // wait time before the loop starts again
    if (deviceConfig.configController.heatingIntervalEnabled == true)
    {
        if(!(millisCurrent - millisStartHeating >= deviceConfig.configController.heatingInterval*1000)){
            return;
        }
    
        int relay1=0;
        int relay2=0;
        int relay3=0;
        int relay4=0;
        int relay5=0;
        int relay6=0;
        int relay7=0;
        int relay8=0;

        if(deviceConfig.configController.relay1==1) { relay1 = deviceConfig.configPin.RELAY_1; } ;// 1 is value of ventilation
        if(deviceConfig.configController.relay2==1) { relay1 = deviceConfig.configPin.RELAY_2; } ;
        if(deviceConfig.configController.relay3==1) { relay1 = deviceConfig.configPin.RELAY_3; } ;
        if(deviceConfig.configController.relay4==1) { relay1 = deviceConfig.configPin.RELAY_4; } ;
        if(deviceConfig.configController.relay5==1) { relay1 = deviceConfig.configPin.RELAY_5; } ;
        if(deviceConfig.configController.relay6==1) { relay1 = deviceConfig.configPin.RELAY_6; } ;
        if(deviceConfig.configController.relay7==1) { relay1 = deviceConfig.configPin.RELAY_7; } ;
        if(deviceConfig.configController.relay8==1) { relay1 = deviceConfig.configPin.RELAY_8; } ;

        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay1, HIGH);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay2, HIGH);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay3, HIGH);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay4, HIGH);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay5, HIGH);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay6, HIGH);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay7, HIGH);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay8, HIGH);};
    
    if(!(millisCurrent - millisStartHeatingLenght >= deviceConfig.configController.heatingInterval*1000)){
            return;
        }

        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay1, LOW);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay2, LOW);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay3, LOW);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay4, LOW);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay5, LOW);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay6, LOW);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay7, LOW);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay8, LOW);};    

    }

}

void ControllerController::intervalWaterPump()
{
millisCurrent = millis();   

    // wait time before the loop starts again
    if (deviceConfig.configController.waterPumpIntervalEnabled == true)
    {
        if(!(millisCurrent - millisStartWaterPump >= deviceConfig.configController.waterPumpInterval*1000)){
            return;
        }
    
        int relay1=0;
        int relay2=0;
        int relay3=0;
        int relay4=0;
        int relay5=0;
        int relay6=0;
        int relay7=0;
        int relay8=0;

        if(deviceConfig.configController.relay1==1) { relay1 = deviceConfig.configPin.RELAY_1; } ;// 1 is value of ventilation
        if(deviceConfig.configController.relay2==1) { relay1 = deviceConfig.configPin.RELAY_2; } ;
        if(deviceConfig.configController.relay3==1) { relay1 = deviceConfig.configPin.RELAY_3; } ;
        if(deviceConfig.configController.relay4==1) { relay1 = deviceConfig.configPin.RELAY_4; } ;
        if(deviceConfig.configController.relay5==1) { relay1 = deviceConfig.configPin.RELAY_5; } ;
        if(deviceConfig.configController.relay6==1) { relay1 = deviceConfig.configPin.RELAY_6; } ;
        if(deviceConfig.configController.relay7==1) { relay1 = deviceConfig.configPin.RELAY_7; } ;
        if(deviceConfig.configController.relay8==1) { relay1 = deviceConfig.configPin.RELAY_8; } ;

        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay1, HIGH);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay2, HIGH);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay3, HIGH);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay4, HIGH);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay5, HIGH);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay6, HIGH);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay7, HIGH);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay8, HIGH);};
    
    if(!(millisCurrent - millisStartWaterPumpLenght >= deviceConfig.configController.waterPumpInterval*1000)){
            return;
        }

        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay1, LOW);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay2, LOW);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay3, LOW);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay4, LOW);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay5, LOW);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay6, LOW);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay7, LOW);};
        if (relay1!=0) {digitalWrite(deviceConfig.configController.relay8, LOW);};    

    }

}



void ControllerController::initController(SensorData sensorData)
{

    // run controller on interval
    intervalVentilation();
    intervalLight();
    intervalHeating();
    intervalWaterPump();

    // run controller on sensor values

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