#ifndef ControllerController_H
#define ControllerController_H
#include "Arduino.h"
#include <ArduinoJson.h>

#include "../Model/DeviceModel.h"
#include "../Controller/DeviceController.h"
#include "../Controller/SensorController.h"
#include "../Controller/ControllerController.h"

class ControllerController
{
public:
    DeviceConfig deviceConfig;

    void setupController();
    void initController(SensorData sensorData);



private:
    void relayVentilation(int relayPin, SensorData sensorData);
    void relayWaterPump(int relayPin, SensorData sensorData);
    void relayHeating(int relayPin, SensorData sensorData);
    void relayLight(int relayPin, SensorData sensorData);
    
    void intervalVentilation();
    void intervalLight();
    void intervalHeating();
    void intervalWaterPump();


};
#endif