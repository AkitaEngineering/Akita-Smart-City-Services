# Akita Smart City Services (ASCS) - Project Requirements

This document outlines the necessary hardware, software, libraries, and infrastructure required to develop, build, deploy, and operate the Akita Smart City Services (ASCS) Meshtastic plugin.

## I. Development Environment

* **IDE:** PlatformIO IDE (Recommended). Can be used as an extension within Visual Studio Code or other compatible editors. PlatformIO simplifies dependency management, building, and uploading firmware.
* **Build System:** PlatformIO Core CLI (automatically used by PlatformIO IDE).
* **Version Control:** Git (for cloning the repository and managing code versions).
* **Nanopb Generator:**
    * Python 3.x installed.
    * Nanopb Python package installed (`pip install nanopb`). Required to generate C code (`.pb.c`, `.pb.h`) from the `.proto` definition file (`SmartCity.proto`).

## II. Core Firmware Libraries (Managed via PlatformIO `lib_deps`)

* **Meshtastic Device Firmware:** The core Meshtastic firmware library.
    * Example `platformio.ini` entry: `meshtastic/Meshtastic-device @ ^2.x.x` (Verify and use the latest compatible version).
* **Nanopb (Runtime Library):** The C runtime library for encoding/decoding Protocol Buffer messages.
    * Example `platformio.ini` entry: `nanopb/nanopb @ ^0.4.7` (Or point to the specific version included in your project structure if not using the library manager version).

## III. Role-Specific Libraries (Gateway Role - Conditional)

These libraries are only required if compiling firmware for nodes intended to operate as **Gateways** (i.e., when the `ASCS_ROLE_GATEWAY` build flag is defined).

* **PubSubClient:** MQTT client library for Arduino.
    * Example `platformio.ini` entry: `knolleary/PubSubClient @ ^2.8`
* **ArduinoJson:** Library for parsing and generating JSON documents (used for MQTT payloads).
    * Example `platformio.ini` entry: `bblanchon/ArduinoJson @ ^6.19.4` (Ensure version compatibility).
* **WiFi:** Provided by the ESP32 core SDK (no separate installation usually needed for ESP32 targets).
* **Filesystem Support (SPIFFS or LittleFS):** Required for Gateway message buffering. Provided by the ESP32 core SDK. Ensure the chosen filesystem (`FileSystem` macro in `AkitaSmartCityServices.cpp`) is correctly initialized in the main application (`main.cpp`).

## IV. Hardware

* **Microcontroller:** ESP32-based LoRa development boards are highly recommended, especially for Gateway nodes due to WiFi requirements. nRF52-based Meshtastic devices can also be used, primarily for Sensor/Aggregator roles.
    * Examples: Heltec LoRa 32 (V2, V3), LILYGO T-Beam, RAK Wireless WisBlock.
* **LoRa Module:** Integrated LoRa transceiver compatible with the Meshtastic firmware (e.g., SX127x, SX126x).
* **Sensors (for Sensor Role):** Any physical sensor compatible with the chosen microcontroller (e.g., BME280, DS18B20, ultrasonic distance sensors, etc.) along with their respective Arduino libraries. An implementation inheriting from `SensorInterface.h` is required.
* **Power Source:** Appropriate power supply for continuous operation (USB, battery with charging circuit, solar).

## V. External Infrastructure (Gateway Role)

* **WiFi Network:** An accessible 2.4 GHz WiFi network with internet connectivity for Gateway nodes.
* **MQTT Broker:** A running MQTT broker (e.g., Mosquitto, HiveMQ, cloud-based MQTT service).
    * Must be accessible by the Gateway nodes over the network.
    * **Security:** It is **highly recommended** to configure the broker for secure connections (TLS) and use strong authentication (e.g., username/password). ASCS currently supports username/password via PubSubClient.

## VI. Configuration

* Each node requires specific configuration parameters set via the Meshtastic `Preferences` system (using Serial Console or Python API) after flashing. Refer to the main `README.md` for configuration keys and procedures.

