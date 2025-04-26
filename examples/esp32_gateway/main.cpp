#include <Meshtastic.h>
#include <Wire.h> // Often needed for board initialization even if no I2C sensor used

// Include the ASCS Plugin header (adjust path if needed)
#include "AkitaSmartCityServices.h"

// --- Include Filesystem library (Required for Gateway Buffering) ---
// This needs to match the 'FileSystem' definition in AkitaSmartCityServices.cpp
// and the board_build.filesystem setting in platformio.ini
#include <SPIFFS.h> // Or LittleFS.h
#define FileSystem SPIFFS // Or LittleFS

// --- Create an instance of the ASCS plugin ---
AkitaSmartCityServices ascsPlugin;

// --- No Sensor Implementation Needed for Gateway Role ---
// We don't call ascsPlugin.setSensor() for a gateway.

// --- Arduino Setup Function ---
void setup() {
    Serial.begin(115200);
    Serial.println("\nMeshtastic ASCS GATEWAY Example Starting...");
    delay(1000); // Allow serial to initialize

    // --- Initialize Filesystem (Required for Gateway Buffering) ---
    // This MUST be done before Meshtastic::begin() which calls plugin::init()
    // where the plugin might try to access the filesystem.
    Serial.println("Initializing Filesystem for Gateway Buffering...");
    // Use formatOnFail=true cautiously - it will erase all data on the filesystem
    // if mounting fails (e.g., first boot or corruption). Set to false for production
    // unless you have a specific recovery strategy.
    if (!FileSystem.begin(false)) {
         Serial.println("Filesystem Mount Failed! Trying to format...");
         // Attempt format only if initial mount fails
         if (!FileSystem.begin(true)) {
             Serial.println("FATAL: Filesystem Format Failed! Gateway buffering disabled.");
             // Consider halting or indicating error state
         } else {
              Serial.println("Filesystem Formatted and Initialized.");
         }
    } else {
         Serial.println("Filesystem Initialized Successfully.");
         // Optional: Check available space
         // Serial.printf("FS Total: %lu, Used: %lu\n", FileSystem.totalBytes(), FileSystem.usedBytes());
    }


    // --- Configure ASCS Plugin BEFORE meshtastic.begin() ---

    // 1. Set Sensor (NOT NEEDED for Gateway)
    //    Do NOT call ascsPlugin.setSensor()

    // 2. Register the plugin with Meshtastic.
    meshtastic.addPlugin(&ascsPlugin);

    // --- Start Meshtastic ---
    Serial.println("Starting Meshtastic...");
    meshtastic.begin();

    Serial.println("Meshtastic initialization complete.");
    // Query the plugin's role (should reflect the value loaded from Preferences, ideally 3)
    Serial.printf("ASCS Plugin Role reported as: %d (1=Sensor, 2=Agg, 3=GW)\n", ascsPlugin.getNodeRole());
    if (ascsPlugin.getNodeRole() != ServiceDiscovery_Role_GATEWAY) {
        Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        Serial.println("!! WARNING: Node role not configured as GATEWAY (3) in    !!");
        Serial.println("!!          Preferences. Check config: '!prefs list ascs' !!");
        Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    }
    Serial.println("Device setup complete. Running as Gateway...");
}

// --- Arduino Loop Function ---
void loop() {
    // Let Meshtastic handle its core loop and plugin callbacks
    meshtastic.loop();

    // Add a small delay
    delay(10);
}
