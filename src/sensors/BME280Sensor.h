#ifndef ASCS_BME280_SENSOR_H
#define ASCS_BME280_SENSOR_H

#ifdef ASCS_SENSOR_BME280

#include "../interfaces/SensorInterface.h"
#include <Adafruit_BME280.h>
#include <cstdint>
#include <string>

class ASCSBME280Sensor final : public SensorInterface {
  public:
    explicit ASCSBME280Sensor(std::string sensorId = "BME280", uint8_t i2cAddress = BME280_ADDRESS_ALTERNATE);
    bool initialize() override;
    bool readData(std::map<std::string, float> &readings) override;
    std::string getSensorId() override;

  private:
    Adafruit_BME280 m_sensor;
    std::string m_sensorId;
    uint8_t m_i2cAddress;
    bool m_initialized = false;
};

#endif

#endif
