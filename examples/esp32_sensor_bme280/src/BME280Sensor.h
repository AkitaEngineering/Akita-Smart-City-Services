#ifndef BME280_SENSOR_H
#define BME280_SENSOR_H

// Include the ASCS Sensor Interface definition
#include "interfaces/SensorInterface.h" // Adjust path if needed

// Include the BME280 library
#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>
#include <Wire.h> // For I2C communication

/**
 * @brief Concrete implementation of SensorInterface for the Adafruit BME280 sensor.
 */
class BME280Sensor : public SensorInterface {
public:
    /**
     * @brief Constructor. Initializes the sensor.
     * @param sensorId A string identifier for this sensor instance.
     * @param i2cAddress The I2C address of the BME280 sensor (default 0x76 or 0x77).
     */
    BME280Sensor(const std::string& sensorId = "BME280", uint8_t i2cAddress = BME280_ADDRESS_ALTERNATE); // Default 0x77

    /**
     * @brief Destructor.
     */
    virtual ~BME280Sensor() = default;

    /**
     * @brief Initializes the BME280 sensor communication.
     * Must be called once in setup() before reading data.
     * @return True if initialization was successful, false otherwise.
     */
    bool initSensor();

    /**
     * @brief Reads temperature, humidity, and pressure data from the BME280.
     * Populates the provided map with standard keys.
     * @param readings Reference to a map where sensor readings will be stored.
     * Keys used: "temperature_c", "humidity_pct", "pressure_pa".
     * @return True if reading was successful, false otherwise.
     */
    virtual bool readData(std::map<std::string, float>& readings) override;

    /**
     * @brief Returns the configured sensor ID string.
     * @return The sensor ID.
     */
    virtual std::string getSensorId() override;

private:
    Adafruit_BME280 bme; // BME280 sensor object
    std::string m_sensorId; // Store the sensor ID
    uint8_t m_i2cAddress; // Store the I2C address
    bool m_initialized; // Flag to track initialization status
};

#endif // BME280_SENSOR_H
