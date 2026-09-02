#ifndef SensorController_H
#define SensorController_H

#include "Arduino.h"
#include "ArduinoJson.h"

#include "../Model/DeviceModel.h"

// Forward declarations instead of includes
class DeviceController;
class ServiceController;

class SensorController
{

private:
    SensorData sensorData;

    void sensor_DHT11_temp();          // 1001
    void sensor_DHT11_humid();          // 1001
    void sensor_DHT22_temp();          // 1002
    void sensor_DHT22_humid();          // 1002
    void sensor_BMP180_temp();         // 1003
    void sensor_BMP180_pres();         // 1003
    void sensor_BMP280_temp();         // 1004
    void sensor_BMP280_pres();         // 1004
    void sensor_BME280_temp();         // 1005
    void sensor_BME280_humid();         // 1005
    void sensor_BME280_pres();         // 1005
    void sensor_CCS811_co2();         // 1006
    void sensor_CCS811_tvoc();         // 1006
    void sensor_DS18B20_temp();        // 1007
    void sensor_BH1750_lux();         // 1008

    void sensor_Wind();
    void sensor_analog_voltage(); // 2001 - roadmap #12: VoltageDivider battery reading (despite the generic pre-#12 name, this IS the battery divider case)
    void sensor_battery_max17048(); // 1009, roadmap #12
    void sensor_analog_moist();   // 2002
    void sensor_liquid_PH(); // unavailable
    void sensor_analog_waterLevel(); // unavailable
    void sensor_rainLevel(); // unavailable

    // Roadmap #9: drains /buffer oldest-first, deleting each file only after its own 2xx.
    // Returns true when the queue is empty on exit; false = broke off mid-queue (connection
    // dropped again), remaining files stay on disk for the next cycle.
    bool flushBufferedSensorData();


public:
    void setupSensor();

    // Roadmap #129: no deviceConfig member here anymore - unqualified `deviceConfig` inside this
    // class's own methods now resolves to the single canonical instance (DeviceModel.h extern).
    // serviceRequest stays a member: unlike deviceConfig, each module legitimately owns its own
    // (this one always targets the sensor-data/event endpoints, main.cpp's own targets auth/config).
    ServiceRequest serviceRequest;

    void buildSensorData(DeviceConfig deviceConfig);
    void buildSensorDataPayload();
    void pushSensorData(JsonDocument payload);
};

// Roadmap #129: the one SensorController instance, defined in main.cpp - see DeviceModel.h's
// deviceConfig/serviceEndpoint externs for the same reasoning.
extern SensorController sensor;

#endif