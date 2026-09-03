#ifndef SensorController_H
#define SensorController_H

#include "Arduino.h"
#include "ArduinoJson.h"
#include <Adafruit_Sensor.h> // sensors_event_t, used by the DHT report helpers below

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
    void sensor_analog_voltage(); // 2001, VoltageDivider
    void sensor_battery_max17048(); // 1009
    void sensor_analog_moist();   // 2002
    void sensor_liquid_PH(); // unavailable
    void sensor_analog_waterLevel(); // unavailable
    void sensor_rainLevel(); // unavailable

    // Shared print/store tail for the DHT11/DHT22, BMP180/BMP280 and CCS811 co2/tvoc pairs - only the read call differs per library.
    void reportDHTTemperature(sensors_event_t &event, const char *label);
    void reportDHTHumidity(sensors_event_t &event, const char *label);
    void reportSensorInitError(const char *label);
    void reportTemperature(double celsius);
    void reportPressure(double pascals);
    bool tryReadCCS811(); // available()+readData(), false on either miss or heat-up wait

    // Drains /buffer oldest-first, deleting each file only after its own 2xx. Returns false if it broke off mid-queue (connection dropped again).
    bool flushBufferedSensorData();


public:
    void setupSensor();

    // serviceRequest stays a per-module member (unlike deviceConfig): this one always targets the sensor-data/event endpoints.
    ServiceRequest serviceRequest;

    void buildSensorData(DeviceConfig deviceConfig);
    void buildSensorDataPayload();
    void pushSensorData(JsonDocument payload);
};

// The one SensorController instance, defined in main.cpp.
extern SensorController sensor;

#endif