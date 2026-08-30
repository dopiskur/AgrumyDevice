#include "arduino.h"
#include <WiFi.h>
#include <EEPROM.h>
#include "FS.h"
#include "WiFiManager.h"

#include "DeviceController.h"
#include "ServiceController.h"
#include "ControllerController.h"

static DeviceDefaults deviceDefaults;
static DeviceConfig deviceConfig;
static ServiceEndpoint serviceEndpoint;

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

// Turns the selected relay(s) on once Interval has elapsed since the last OFF, holds them on
// until IntervalLenght has elapsed since that ON, then repeats - a proper duty cycle instead of
// firing and immediately reversing within the same call (roadmap #44). State is read back from
// the first selected pin via digitalRead() rather than a separate bool, same approach the
// threshold/hysteresis relay functions use (#10) - no separate "is it on" variable that could
// drift out of sync with the actual pin. Relay-type numbers must match deviceTypeRelay's seed
// order (1=Ventilation, 2=Light, 3=Heating, 4=Water pump), same as initController()'s switch.
void ControllerController::intervalVentilation()
{
    millisCurrent = millis();

    int relay1=0;
    int relay2=0;
    int relay3=0;
    int relay4=0;
    int relay5=0;
    int relay6=0;
    int relay7=0;
    int relay8=0;

    if(deviceConfig.configController.relay1==1) { relay1 = deviceConfig.configPin.RELAY_1; } ;
    if(deviceConfig.configController.relay2==1) { relay2 = deviceConfig.configPin.RELAY_2; } ;
    if(deviceConfig.configController.relay3==1) { relay3 = deviceConfig.configPin.RELAY_3; } ;
    if(deviceConfig.configController.relay4==1) { relay4 = deviceConfig.configPin.RELAY_4; } ;
    if(deviceConfig.configController.relay5==1) { relay5 = deviceConfig.configPin.RELAY_5; } ;
    if(deviceConfig.configController.relay6==1) { relay6 = deviceConfig.configPin.RELAY_6; } ;
    if(deviceConfig.configController.relay7==1) { relay7 = deviceConfig.configPin.RELAY_7; } ;
    if(deviceConfig.configController.relay8==1) { relay8 = deviceConfig.configPin.RELAY_8; } ;

    if (!deviceConfig.configController.ventilationIntervalEnabled)
    {
        return;
    }

    // Multiple slots assigned to this function move together, keyed off the first one found.
    int referencePin = relay1 ? relay1 : relay2 ? relay2 : relay3 ? relay3 : relay4 ? relay4
                      : relay5 ? relay5 : relay6 ? relay6 : relay7 ? relay7 : relay8;
    if (referencePin == 0)
    {
        return; // no relay slot assigned to Ventilation
    }
    pinMode(referencePin, OUTPUT);
    bool isCurrentlyOn = digitalRead(referencePin) == HIGH;

    if (!isCurrentlyOn && (millisCurrent - millisStartVentilation >= (unsigned long)deviceConfig.configController.ventilationInterval*1000))
    {
        if (relay1!=0) {digitalWrite(relay1, HIGH);};
        if (relay2!=0) {digitalWrite(relay2, HIGH);};
        if (relay3!=0) {digitalWrite(relay3, HIGH);};
        if (relay4!=0) {digitalWrite(relay4, HIGH);};
        if (relay5!=0) {digitalWrite(relay5, HIGH);};
        if (relay6!=0) {digitalWrite(relay6, HIGH);};
        if (relay7!=0) {digitalWrite(relay7, HIGH);};
        if (relay8!=0) {digitalWrite(relay8, HIGH);};
        millisStartVentilationLenght = millisCurrent;
    }
    else if (isCurrentlyOn && (millisCurrent - millisStartVentilationLenght >= (unsigned long)deviceConfig.configController.ventilationIntervalLenght*1000))
    {
        if (relay1!=0) {digitalWrite(relay1, LOW);};
        if (relay2!=0) {digitalWrite(relay2, LOW);};
        if (relay3!=0) {digitalWrite(relay3, LOW);};
        if (relay4!=0) {digitalWrite(relay4, LOW);};
        if (relay5!=0) {digitalWrite(relay5, LOW);};
        if (relay6!=0) {digitalWrite(relay6, LOW);};
        if (relay7!=0) {digitalWrite(relay7, LOW);};
        if (relay8!=0) {digitalWrite(relay8, LOW);};
        millisStartVentilation = millisCurrent;
    }
}

void ControllerController::intervalLight()
{
    millisCurrent = millis();

    int relay1=0;
    int relay2=0;
    int relay3=0;
    int relay4=0;
    int relay5=0;
    int relay6=0;
    int relay7=0;
    int relay8=0;

    if(deviceConfig.configController.relay1==2) { relay1 = deviceConfig.configPin.RELAY_1; } ;
    if(deviceConfig.configController.relay2==2) { relay2 = deviceConfig.configPin.RELAY_2; } ;
    if(deviceConfig.configController.relay3==2) { relay3 = deviceConfig.configPin.RELAY_3; } ;
    if(deviceConfig.configController.relay4==2) { relay4 = deviceConfig.configPin.RELAY_4; } ;
    if(deviceConfig.configController.relay5==2) { relay5 = deviceConfig.configPin.RELAY_5; } ;
    if(deviceConfig.configController.relay6==2) { relay6 = deviceConfig.configPin.RELAY_6; } ;
    if(deviceConfig.configController.relay7==2) { relay7 = deviceConfig.configPin.RELAY_7; } ;
    if(deviceConfig.configController.relay8==2) { relay8 = deviceConfig.configPin.RELAY_8; } ;

    if (!deviceConfig.configController.lightIntervalEnabled)
    {
        return;
    }

    int referencePin = relay1 ? relay1 : relay2 ? relay2 : relay3 ? relay3 : relay4 ? relay4
                      : relay5 ? relay5 : relay6 ? relay6 : relay7 ? relay7 : relay8;
    if (referencePin == 0)
    {
        return; // no relay slot assigned to Light
    }
    pinMode(referencePin, OUTPUT);
    bool isCurrentlyOn = digitalRead(referencePin) == HIGH;

    if (!isCurrentlyOn && (millisCurrent - millisStartLight >= (unsigned long)deviceConfig.configController.lightInterval*1000))
    {
        if (relay1!=0) {digitalWrite(relay1, HIGH);};
        if (relay2!=0) {digitalWrite(relay2, HIGH);};
        if (relay3!=0) {digitalWrite(relay3, HIGH);};
        if (relay4!=0) {digitalWrite(relay4, HIGH);};
        if (relay5!=0) {digitalWrite(relay5, HIGH);};
        if (relay6!=0) {digitalWrite(relay6, HIGH);};
        if (relay7!=0) {digitalWrite(relay7, HIGH);};
        if (relay8!=0) {digitalWrite(relay8, HIGH);};
        millisStartLightLenght = millisCurrent;
    }
    else if (isCurrentlyOn && (millisCurrent - millisStartLightLenght >= (unsigned long)deviceConfig.configController.lightIntervalLenght*1000))
    {
        if (relay1!=0) {digitalWrite(relay1, LOW);};
        if (relay2!=0) {digitalWrite(relay2, LOW);};
        if (relay3!=0) {digitalWrite(relay3, LOW);};
        if (relay4!=0) {digitalWrite(relay4, LOW);};
        if (relay5!=0) {digitalWrite(relay5, LOW);};
        if (relay6!=0) {digitalWrite(relay6, LOW);};
        if (relay7!=0) {digitalWrite(relay7, LOW);};
        if (relay8!=0) {digitalWrite(relay8, LOW);};
        millisStartLight = millisCurrent;
    }
}

void ControllerController::intervalHeating()
{
    millisCurrent = millis();

    int relay1=0;
    int relay2=0;
    int relay3=0;
    int relay4=0;
    int relay5=0;
    int relay6=0;
    int relay7=0;
    int relay8=0;

    if(deviceConfig.configController.relay1==3) { relay1 = deviceConfig.configPin.RELAY_1; } ;
    if(deviceConfig.configController.relay2==3) { relay2 = deviceConfig.configPin.RELAY_2; } ;
    if(deviceConfig.configController.relay3==3) { relay3 = deviceConfig.configPin.RELAY_3; } ;
    if(deviceConfig.configController.relay4==3) { relay4 = deviceConfig.configPin.RELAY_4; } ;
    if(deviceConfig.configController.relay5==3) { relay5 = deviceConfig.configPin.RELAY_5; } ;
    if(deviceConfig.configController.relay6==3) { relay6 = deviceConfig.configPin.RELAY_6; } ;
    if(deviceConfig.configController.relay7==3) { relay7 = deviceConfig.configPin.RELAY_7; } ;
    if(deviceConfig.configController.relay8==3) { relay8 = deviceConfig.configPin.RELAY_8; } ;

    if (!deviceConfig.configController.heatingIntervalEnabled)
    {
        return;
    }

    int referencePin = relay1 ? relay1 : relay2 ? relay2 : relay3 ? relay3 : relay4 ? relay4
                      : relay5 ? relay5 : relay6 ? relay6 : relay7 ? relay7 : relay8;
    if (referencePin == 0)
    {
        return; // no relay slot assigned to Heating
    }
    pinMode(referencePin, OUTPUT);
    bool isCurrentlyOn = digitalRead(referencePin) == HIGH;

    if (!isCurrentlyOn && (millisCurrent - millisStartHeating >= (unsigned long)deviceConfig.configController.heatingInterval*1000))
    {
        if (relay1!=0) {digitalWrite(relay1, HIGH);};
        if (relay2!=0) {digitalWrite(relay2, HIGH);};
        if (relay3!=0) {digitalWrite(relay3, HIGH);};
        if (relay4!=0) {digitalWrite(relay4, HIGH);};
        if (relay5!=0) {digitalWrite(relay5, HIGH);};
        if (relay6!=0) {digitalWrite(relay6, HIGH);};
        if (relay7!=0) {digitalWrite(relay7, HIGH);};
        if (relay8!=0) {digitalWrite(relay8, HIGH);};
        millisStartHeatingLenght = millisCurrent;
    }
    else if (isCurrentlyOn && (millisCurrent - millisStartHeatingLenght >= (unsigned long)deviceConfig.configController.heatingIntervalLenght*1000))
    {
        if (relay1!=0) {digitalWrite(relay1, LOW);};
        if (relay2!=0) {digitalWrite(relay2, LOW);};
        if (relay3!=0) {digitalWrite(relay3, LOW);};
        if (relay4!=0) {digitalWrite(relay4, LOW);};
        if (relay5!=0) {digitalWrite(relay5, LOW);};
        if (relay6!=0) {digitalWrite(relay6, LOW);};
        if (relay7!=0) {digitalWrite(relay7, LOW);};
        if (relay8!=0) {digitalWrite(relay8, LOW);};
        millisStartHeating = millisCurrent;
    }
}

void ControllerController::intervalWaterPump()
{
    millisCurrent = millis();

    int relay1=0;
    int relay2=0;
    int relay3=0;
    int relay4=0;
    int relay5=0;
    int relay6=0;
    int relay7=0;
    int relay8=0;

    if(deviceConfig.configController.relay1==4) { relay1 = deviceConfig.configPin.RELAY_1; } ;
    if(deviceConfig.configController.relay2==4) { relay2 = deviceConfig.configPin.RELAY_2; } ;
    if(deviceConfig.configController.relay3==4) { relay3 = deviceConfig.configPin.RELAY_3; } ;
    if(deviceConfig.configController.relay4==4) { relay4 = deviceConfig.configPin.RELAY_4; } ;
    if(deviceConfig.configController.relay5==4) { relay5 = deviceConfig.configPin.RELAY_5; } ;
    if(deviceConfig.configController.relay6==4) { relay6 = deviceConfig.configPin.RELAY_6; } ;
    if(deviceConfig.configController.relay7==4) { relay7 = deviceConfig.configPin.RELAY_7; } ;
    if(deviceConfig.configController.relay8==4) { relay8 = deviceConfig.configPin.RELAY_8; } ;

    if (!deviceConfig.configController.waterPumpIntervalEnabled)
    {
        return;
    }

    int referencePin = relay1 ? relay1 : relay2 ? relay2 : relay3 ? relay3 : relay4 ? relay4
                      : relay5 ? relay5 : relay6 ? relay6 : relay7 ? relay7 : relay8;
    if (referencePin == 0)
    {
        return; // no relay slot assigned to Water pump
    }
    pinMode(referencePin, OUTPUT);
    bool isCurrentlyOn = digitalRead(referencePin) == HIGH;

    if (!isCurrentlyOn && (millisCurrent - millisStartWaterPump >= (unsigned long)deviceConfig.configController.waterPumpInterval*1000))
    {
        if (relay1!=0) {digitalWrite(relay1, HIGH);};
        if (relay2!=0) {digitalWrite(relay2, HIGH);};
        if (relay3!=0) {digitalWrite(relay3, HIGH);};
        if (relay4!=0) {digitalWrite(relay4, HIGH);};
        if (relay5!=0) {digitalWrite(relay5, HIGH);};
        if (relay6!=0) {digitalWrite(relay6, HIGH);};
        if (relay7!=0) {digitalWrite(relay7, HIGH);};
        if (relay8!=0) {digitalWrite(relay8, HIGH);};
        millisStartWaterPumpLenght = millisCurrent;
    }
    else if (isCurrentlyOn && (millisCurrent - millisStartWaterPumpLenght >= (unsigned long)deviceConfig.configController.waterPumpIntervalLenght*1000))
    {
        if (relay1!=0) {digitalWrite(relay1, LOW);};
        if (relay2!=0) {digitalWrite(relay2, LOW);};
        if (relay3!=0) {digitalWrite(relay3, LOW);};
        if (relay4!=0) {digitalWrite(relay4, LOW);};
        if (relay5!=0) {digitalWrite(relay5, LOW);};
        if (relay6!=0) {digitalWrite(relay6, LOW);};
        if (relay7!=0) {digitalWrite(relay7, LOW);};
        if (relay8!=0) {digitalWrite(relay8, LOW);};
        millisStartWaterPump = millisCurrent;
    }
}



// Case numbers must match deviceTypeRelay's IDDeviceTypeRelay seed order (db/agrumyDB-final.sql:
// 1=Ventilation, 2=Light, 3=Heating, 4=Water pump) - the Web admin dropdown stores that ID
// directly into relay1..relay8, so a mismatch here silently runs the wrong relay function.
void ControllerController::initController(SensorData sensorData)
{

    intervalVentilation();
    intervalLight();
    intervalHeating();
    intervalWaterPump();

    switch (deviceConfig.configController.relay1)
    {
    case 0:
        break;
    case 1:
        relayVentilation(deviceConfig.configPin.RELAY_1, sensorData);
        break;

    case 2:
        relayLight(deviceConfig.configPin.RELAY_1, sensorData);
        break;

    case 3:
        relayHeating(deviceConfig.configPin.RELAY_1, sensorData);
        break;

    case 4:
        relayWaterPump(deviceConfig.configPin.RELAY_1, sensorData);
        break;

    default:
        break;
    }

    switch (deviceConfig.configController.relay2)
    {
    case 0:
        break;
    case 1:
        relayVentilation(deviceConfig.configPin.RELAY_2, sensorData);
        break;

    case 2:
        relayLight(deviceConfig.configPin.RELAY_2, sensorData);
        break;

    case 3:
        relayHeating(deviceConfig.configPin.RELAY_2, sensorData);
        break;

    case 4:
        relayWaterPump(deviceConfig.configPin.RELAY_2, sensorData);
        break;

    default:
        break;
    }

    switch (deviceConfig.configController.relay3)
    {
    case 0:
        break;
    case 1:
        relayVentilation(deviceConfig.configPin.RELAY_3, sensorData);
        break;

    case 2:
        relayLight(deviceConfig.configPin.RELAY_3, sensorData);
        break;

    case 3:
        relayHeating(deviceConfig.configPin.RELAY_3, sensorData);
        break;

    case 4:
        relayWaterPump(deviceConfig.configPin.RELAY_3, sensorData);
        break;

    default:
        break;
    }

    switch (deviceConfig.configController.relay4)
    {
    case 0:
        break;
    case 1:
        relayVentilation(deviceConfig.configPin.RELAY_4, sensorData);
        break;

    case 2:
        relayLight(deviceConfig.configPin.RELAY_4, sensorData);
        break;

    case 3:
        relayHeating(deviceConfig.configPin.RELAY_4, sensorData);
        break;

    case 4:
        relayWaterPump(deviceConfig.configPin.RELAY_4, sensorData);
        break;

    default:
        break;
    }

    switch (deviceConfig.configController.relay5)
    {
    case 0:
        break;
    case 1:
        relayVentilation(deviceConfig.configPin.RELAY_5, sensorData);
        break;

    case 2:
        relayLight(deviceConfig.configPin.RELAY_5, sensorData);
        break;

    case 3:
        relayHeating(deviceConfig.configPin.RELAY_5, sensorData);
        break;

    case 4:
        relayWaterPump(deviceConfig.configPin.RELAY_5, sensorData);
        break;

    default:
        break;
    }

    switch (deviceConfig.configController.relay6)
    {
    case 0:
        break;
    case 1:
        relayVentilation(deviceConfig.configPin.RELAY_6, sensorData);
        break;

    case 2:
        relayLight(deviceConfig.configPin.RELAY_6, sensorData);
        break;

    case 3:
        relayHeating(deviceConfig.configPin.RELAY_6, sensorData);
        break;

    case 4:
        relayWaterPump(deviceConfig.configPin.RELAY_6, sensorData);
        break;

    default:
        break;
    }

    switch (deviceConfig.configController.relay7)
    {
    case 0:
        break;
    case 1:
        relayVentilation(deviceConfig.configPin.RELAY_7, sensorData);
        break;

    case 2:
        relayLight(deviceConfig.configPin.RELAY_7, sensorData);
        break;

    case 3:
        relayHeating(deviceConfig.configPin.RELAY_7, sensorData);
        break;

    case 4:
        relayWaterPump(deviceConfig.configPin.RELAY_7, sensorData);
        break;

    default:
        break;
    }

    switch (deviceConfig.configController.relay8)
    {
    case 0:
        break;
    case 1:
        relayVentilation(deviceConfig.configPin.RELAY_8, sensorData);
        break;

    case 2:
        relayLight(deviceConfig.configPin.RELAY_8, sensorData);
        break;

    case 3:
        relayHeating(deviceConfig.configPin.RELAY_8, sensorData);
        break;

    case 4:
        relayWaterPump(deviceConfig.configPin.RELAY_8, sensorData);
        break;

    default:
        break;
    }
}

void ControllerController::relayVentilation(int relayPin, SensorData sensorData)
{
    const int powerPin = relayPin;
    pinMode(powerPin, OUTPUT);

    double currentHumidity = atof(sensorData.humidity.c_str()); // reading, not to be confused with configController.humidHigh (threshold)
    bool isCurrentlyOn = digitalRead(powerPin) == HIGH;

    // Opposite direction from the other three relay functions: ventilation turns ON above
    // the threshold (too humid), so the dead zone sits BELOW humidHigh, not above it. Turn on
    // only when above humidHigh, turn off only once it drops back below humidHigh - hysteresis.
    if (!isCurrentlyOn && currentHumidity > deviceConfig.configController.humidHigh)
    {
        digitalWrite(powerPin, HIGH);
        Serial.println("[Power rail on]");
    }
    else if (isCurrentlyOn && currentHumidity <= deviceConfig.configController.humidHigh - deviceConfig.configController.humidityHysteresis)
    {
        digitalWrite(powerPin, LOW);
        Serial.println("[Power rail off]");
    }

    delay(500);
};

void ControllerController::relayWaterPump(int relayPin, SensorData sensorData)
{
    const int powerPin = relayPin;
    pinMode(powerPin, OUTPUT);

    double waterLevel = atof(sensorData.waterLevel.c_str());
    bool isCurrentlyOn = digitalRead(powerPin) == HIGH;

    // Dead zone: turn on only when below waterLow, turn off only once it climbs back above
    // waterLow + hysteresis. Reading the pin instead of tracking a separate on/off variable
    // means two relays sharing this function (e.g. relay1 and relay5 both "water pump") each
    // hold their own state correctly, keyed by their own physical pin.
    if (!isCurrentlyOn && waterLevel < deviceConfig.configController.waterLow)
    {
        digitalWrite(powerPin, HIGH);
        Serial.println("[Power rail on]");
    }
    else if (isCurrentlyOn && waterLevel >= deviceConfig.configController.waterLow + deviceConfig.configController.waterLevelHysteresis)
    {
        digitalWrite(powerPin, LOW);
        Serial.println("[Power rail off]");
    }
    delay(500);
};

void ControllerController::relayHeating(int relayPin, SensorData sensorData)
{
    const int powerPin = relayPin;
    pinMode(powerPin, OUTPUT);

    double temperature = atof(sensorData.temperature.c_str());
    bool isCurrentlyOn = digitalRead(powerPin) == HIGH;

    // Dead zone: turn on only when below tempLow, turn off only once it climbs back above
    // tempLow + hysteresis.
    if (!isCurrentlyOn && temperature < deviceConfig.configController.tempLow)
    {
        digitalWrite(powerPin, HIGH);
        Serial.println("[Power rail on]");
    }
    else if (isCurrentlyOn && temperature >= deviceConfig.configController.tempLow + deviceConfig.configController.temperatureHysteresis)
    {
        digitalWrite(powerPin, LOW);
        Serial.println("[Power rail off]");
    }
    delay(500);
};

void ControllerController::relayLight(int relayPin, SensorData sensorData)
{
    const int powerPin = relayPin;
    pinMode(powerPin, OUTPUT);

    double currentLight = atof(sensorData.light.c_str()); // reading, not to be confused with configController.lightLow (threshold)
    bool isCurrentlyOn = digitalRead(powerPin) == HIGH;

    // Dead zone: turn on only when below lightLow, turn off only once it climbs back above
    // lightLow + hysteresis.
    if (!isCurrentlyOn && currentLight < deviceConfig.configController.lightLow)
    {
        digitalWrite(powerPin, HIGH);
        Serial.println("[Power rail on]");
    }
    else if (isCurrentlyOn && currentLight >= deviceConfig.configController.lightLow + deviceConfig.configController.lightHysteresis)
    {
        digitalWrite(powerPin, LOW);
        Serial.println("[Power rail off]");
    }
    delay(500);
};