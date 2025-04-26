#include <Meshtastic.h>
#include <SPI.h> // Include necessary hardware libraries for your sensor

// Include the ASCS Plugin header
#include "AkitaSmartCityServices.h"
// Include your concrete sensor implementation header
// #include "MyBME280Sensor.h" // Example

// --- Create a Concrete Sensor Implementation ---
// (You would put this in its own .h/.cpp file ideally)
class DummySensor : public SensorInterface {
private:
    std::string id = "DummySensor-01";
    float temp = 20.0;
    float humidity = 50.0;
    float battery = 3.9;
public:
    DummySensor(const std::string& sensorId = "DummySensor-01") : id(sensorId) {}

    bool readData(std::map<std::string, float>& readings) override {
        // Simulate reading data
        readings.clear(); // Clear previous readings
        temp += (random(-10, 11) / 10.0); // Random walk
        humidity += (random(-20, 21) / 10.0);
        battery -= 0.001;
        if (humidity < 0) humidity = 0;
        if (humidity > 100) humidity = 100;
        if (battery < 3.0) battery = 4.0; // Recharge! :)

        readings["temperature_c"] = temp;
        readings["humidity_pct"] = humidity;
        readings["battery_v"] = battery;
        readings["random_val"] = random(0, 1000) / 100.0;

        Serial.printf("DummySensor: Read Temp=%.1fC, Hum=%.1f%%, Batt=%.2fV\n",
            readings["temperature_c"], readings["humidity_pct"], readings["battery_v"]);

        return true; // Success
    }

    std::string getSensorId() override {
        return id;
    }
};
// --- End of Sensor Implementation ---


// Create an instance of the plugin
AkitaSmartCityServices ascsPlugin;

// Create an instance of your sensor implementation
// Use std::make_unique for proper memory management
// MyBME280Sensor mySensor(...); // Example for a real sensor
std::unique_ptr<SensorInterface> mySensor = std::make_unique<DummySensor>("TestBench-Sensor");


void setup() {
    Serial.begin(115200);
    Serial.println("Meshtastic ASCS Example Starting...");

    // --- IMPORTANT: Configure ASCS Plugin BEFORE meshtastic.begin() ---

    // 1. Set the sensor implementation (only needed for Sensor role)
    //    Check the configured role *before* setting the sensor if optimizing memory,
    //    but it's safe to set it anyway. The plugin won't use it if not in Sensor role.
    ascsPlugin.setSensor(std::move(mySensor)); // Plugin takes ownership

    // 2. Register the plugin with Meshtastic
    //    Meshtastic will call plugin.init() and plugin.loop() automatically.
    meshtastic.addPlugin(&ascsPlugin);

    // --- Start Meshtastic ---
    // This will initialize the radio, load config, and call plugin.init()
    meshtastic.begin();

    Serial.println("Meshtastic initialization complete.");
    // You can query the plugin's role after meshtastic.begin() has run init()
    Serial.printf("ASCS Plugin Role: %d\n", ascsPlugin.getNodeRole());
}

void loop() {
    // Let Meshtastic handle its core loop and plugin callbacks
    meshtastic.loop();
}

// --- Optional: PlatformIO Build Flags ---
// Add these to your platformio.ini to enable Gateway features
// build_flags =
//   -D ASCS_ROLE_GATEWAY

// Ensure required libraries are included via lib_deps:
// lib_deps =
//   meshtastic/Meshtastic-device @ ^2.x.x // Use correct version
//   knolleary/PubSubClient @ ^2.8
//   bblanchon/ArduinoJson @ ^6.19.4 // Or compatible version
//   nanopb/nanopb @ ^0.4.7 // For protobuf

// You also need to generate the .pb.c and .pb.h files from SmartCity.proto
// using the nanopb generator and include them in your build.
// See PlatformIO documentation for custom build steps or nanopb examples.

