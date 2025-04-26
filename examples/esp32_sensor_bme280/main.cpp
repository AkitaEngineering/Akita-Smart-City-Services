#include <Meshtastic.h>
#include <Wire.h> // Needed for I2C

// Include the ASCS Plugin header (adjust path if needed)
#include "AkitaSmartCityServices.h"
// Include the concrete BME280 sensor implementation
#include "BME280Sensor.h"

// --- Create an instance of the ASCS plugin ---
AkitaSmartCityServices ascsPlugin;

// --- Create an instance of your sensor implementation ---
// Use std::make_unique for proper memory management.
// Provide a unique ID for this sensor node.
std::unique_ptr<SensorInterface> myNodeSensor = std::make_unique<BME280Sensor>("EnvSensor-Node01");

// --- Arduino Setup Function ---
void setup() {
    Serial.begin(115200);
    Serial.println("\nMeshtastic ASCS BME280 Sensor Example Starting...");
    delay(1000); // Allow serial to initialize

    // --- Initialize I2C Communication (Required by BME280) ---
    // Adjust pins if needed for your specific ESP32 board
    Wire.begin(); // Default SDA/SCL pins for most ESP32 boards

    // --- Initialize the BME280 Sensor ---
    // We need to cast the unique_ptr back to the concrete type to call initSensor()
    // This is safe because we know we created a BME280Sensor.
    BME280Sensor* bmePtr = static_cast<BME280Sensor*>(myNodeSensor.get());
    if (!bmePtr->initSensor()) {
         Serial.println("FATAL: BME280 Sensor Initialization Failed! Halting.");
         // Consider adding error handling like blinking an LED or specific behavior
         while(1) { delay(1000); } // Halt execution
    }
     Serial.println("BME280 Initialized.");

    // --- Configure ASCS Plugin BEFORE meshtastic.begin() ---

    // 1. Set the sensor implementation for the plugin.
    //    The plugin takes ownership of the unique_ptr.
    ascsPlugin.setSensor(std::move(myNodeSensor));

    // 2. Register the plugin with Meshtastic.
    //    Meshtastic firmware will automatically call plugin.init() and plugin.loop().
    meshtastic.addPlugin(&ascsPlugin);

    // --- Start Meshtastic ---
    // This initializes the radio, loads config (including ASCS prefs), and calls plugin.init()
    Serial.println("Starting Meshtastic...");
    meshtastic.begin();

    Serial.println("Meshtastic initialization complete.");
    // Query the plugin's role (should reflect the value loaded from Preferences)
    Serial.printf("ASCS Plugin Role reported as: %d (1=Sensor, 2=Agg, 3=GW)\n", ascsPlugin.getNodeRole());
    Serial.println("Device setup complete. Running...");
}

// --- Arduino Loop Function ---
void loop() {
    // Let Meshtastic handle its core loop (radio processing, mesh logic)
    // and call registered plugin loop() methods (like ascsPlugin.loop())
    meshtastic.loop();

    // Add a small delay if your loop does very little else,
    // to prevent overly tight loops if Meshtastic's loop yields quickly.
    // This helps prevent watchdog timeouts on some platforms if Meshtastic::loop()
    // doesn't yield often enough on its own. Adjust delay as needed.
    delay(10);
}
