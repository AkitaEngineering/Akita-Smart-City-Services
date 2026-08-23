#ifdef ASCS_SENSOR_BME280
#include "BME280Sensor.h"
#include "DebugConfiguration.h"
#include <Wire.h>
#include <cctype>
#include <cmath>
#include <utility>

ASCSBME280Sensor::ASCSBME280Sensor(std::string sensorId, uint8_t i2cAddress)
    : m_sensorId(std::move(sensorId)), m_i2cAddress(i2cAddress) {}

bool ASCSBME280Sensor::initialize() {
    if (m_sensorId.empty() || m_sensorId.size() > 64) {
        LOG_ERROR("BME280 sensor identifier must contain between 1 and 64 characters");
        return false;
    }
    for (const unsigned char character : m_sensorId) {
        if (!std::isalnum(character) && character != '-' && character != '_' && character != '.') {
            LOG_ERROR("BME280 sensor identifier contains an unsafe character");
            return false;
        }
    }
    m_initialized = m_sensor.begin(m_i2cAddress, &Wire);
    if (!m_initialized) LOG_ERROR("BME280 was not detected at I2C address 0x%02x", m_i2cAddress);
    return m_initialized;
}

bool ASCSBME280Sensor::readData(std::map<std::string, float> &readings) {
    if (!m_initialized) return false;
    const float temperature = m_sensor.readTemperature();
    const float humidity = m_sensor.readHumidity();
    const float pressure = m_sensor.readPressure();
    if (!std::isfinite(temperature) || !std::isfinite(humidity) || !std::isfinite(pressure) ||
        temperature < -40.0f || temperature > 85.0f || humidity < 0.0f || humidity > 100.0f ||
        pressure < 30000.0f || pressure > 110000.0f) {
        LOG_ERROR("BME280 returned a non-finite or out-of-range reading");
        return false;
    }
    readings.clear();
    readings.emplace("temperature_c", temperature);
    readings.emplace("humidity_pct", humidity);
    readings.emplace("pressure_pa", pressure);
    return true;
}

std::string ASCSBME280Sensor::getSensorId() { return m_sensorId; }
#endif
