#ifndef SENSOR_INTERFACE_H
#define SENSOR_INTERFACE_H

#include "SmartCity.pb.h" // Include the generated header from SmartCity.proto
#include <map>
#include <string>

/**
 * @brief Abstract base class for sensor implementations.
 *
 * This allows the main plugin to interact with different sensor types
 * through a common interface. Implementations should inherit from this
 * class and override the readData method.
 */
class SensorInterface {
public:
    virtual ~SensorInterface() = default; // Virtual destructor

    /**
     * @brief Reads data from the physical sensor(s).
     *
     * @param readings A map to be populated with sensor readings (key: reading name, value: reading value).
     * The implementation should clear the map before adding new readings.
     * @return true if reading was successful, false otherwise.
     */
    virtual bool readData(std::map<std::string, float>& readings) = 0;

    /**
     * @brief Gets the specific ID or name for this sensor instance.
     *
     * This allows distinguishing data if multiple sensors are attached or
     * providing context (e.g., "BME280-LivingRoom").
     *
     * @return A string identifying the sensor. Can be empty if not needed.
     */
    virtual std::string getSensorId() = 0;
};

#endif // SENSOR_INTERFACE_H
