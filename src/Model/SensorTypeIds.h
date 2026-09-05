#ifndef SensorTypeIds_H
#define SensorTypeIds_H

// Canonical deviceTypeSensor IDs - must match Agrumy.Shared's api.Models.SensorTypeIds exactly, renumbering desyncs the two independently-versioned repos.
namespace SensorTypeIds
{
    constexpr int Dht11 = 1001;
    constexpr int Dht22 = 1002;
    constexpr int Bmp180 = 1003;
    constexpr int Bmp280 = 1004;
    constexpr int Bme280 = 1005;
    constexpr int Ccs811 = 1006;
    constexpr int Ds18B20 = 1007;
    constexpr int Bh1750 = 1008;
    constexpr int Max17048 = 1009;
    constexpr int AnalogVoltage = 2001;
    constexpr int AnalogMoisture = 2002;
    constexpr int AnalogWaterLevel = 2003;
}

#endif
