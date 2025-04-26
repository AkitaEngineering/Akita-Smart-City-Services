#include "BME280Sensor.h"
#include "plugin_api.h" // For Log definition

// Constructor
BME280Sensor::BME280Sensor(const std::string& sensorId, uint8_t i2cAddress)
    : m_sensorId(sensorId), m_i2cAddress(i2cAddress), m_initialized(false) {
    // The Adafruit_BME280 object 'bme' is initialized in the header
}

// Initialize the sensor
bool BME280Sensor::initSensor() {
    Log.printf(LOG_LEVEL_INFO, "[BME280Sensor] Initializing BME280 at I2C address 0x%X...\n", m_i2cAddress);
    // Attempt to initialize the sensor using the specified I2C address
    // Wire must be initialized before this (e.g., Wire.begin())
    if (!bme.begin(m_i2cAddress, &Wire)) {
        Log.println(LOG_LEVEL_ERROR, "[BME280Sensor] Could not find a valid BME280 sensor, check wiring and address!");
        m_initialized = false;
        return false;
    }

    // Optional: Configure sensor settings (oversampling, filtering, mode)
    // bme.setSampling(Adafruit_BME280::MODE_NORMAL,     /* Operating Mode. */
    //                 Adafruit_BME280::SAMPLING_X1,     /* Temp. oversampling */
    //                 Adafruit_BME280::SAMPLING_X1,     /* Pressure oversampling */
    //                 Adafruit_BME280::SAMPLING_X1,     /* Humidity oversampling */
    //                 Adafruit_BME280::FILTER_OFF,      /* Filtering. */
    //                 Adafruit_BME280::STANDBY_MS_1000); /* Standby time. */

    Log.println(LOG_LEVEL_INFO, "[BME280Sensor] BME280 Initialized Successfully.");
    m_initialized = true;
    return true;
}

// Read data from the sensor
bool BME280Sensor::readData(std::map<std::string, float>& readings) {
    // Check if the sensor was initialized successfully
    if (!m_initialized) {
        Log.println(LOG_LEVEL_ERROR, "[BME280Sensor] Cannot read data: Sensor not initialized.");
        return false;
    }

    Log.println(LOG_LEVEL_DEBUG, "[BME280Sensor] Reading data...");
    readings.clear(); // Clear any previous readings

    // Read values from the sensor
    float temperature = bme.readTemperature(); // Reads temperature in Celsius
    float humidity = bme.readHumidity();       // Reads relative humidity in %
    float pressure = bme.readPressure();       // Reads pressure in Pascals

    // Basic validation (BME280 can return NaN if read fails)
    if (isnan(temperature) || isnan(humidity) || isnan(pressure)) {
        Log.println(LOG_LEVEL_ERROR, "[BME280Sensor] Failed to read values from BME280 sensor!");
        return false; // Indicate failure
    }

    // Populate the map with standard keys
    try {
        readings["temperature_c"] = temperature;
        readings["humidity_pct"] = humidity;
        readings["pressure_pa"] = pressure;
        // You could also add altitude if needed: readings["altitude_m"] = bme.readAltitude(SEALEVELPRESSURE_HPA);
    } catch (const std::bad_alloc& e) {
        Log.println(LOG_LEVEL_ERROR, "[BME280Sensor] Failed to allocate memory for readings map!");
        return false;
    }


    Log.printf(LOG_LEVEL_DEBUG, "[BME280Sensor] Read: Temp=%.2f C, Hum=%.2f %%, Pres=%.0f Pa\n",
               temperature, humidity, pressure);

    return true; // Indicate success
}

// Get the sensor ID
std::string BME280Sensor::getSensorId() {
    return m_sensorId;
}
