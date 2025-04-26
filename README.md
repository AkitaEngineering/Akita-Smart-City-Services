# Akita Smart City Services (ASCS) - Meshtastic Plugin

**Project Goal:** To provide a robust, scalable, and maintainable Meshtastic plugin for deploying and managing distributed IoT networks essential for smart city services. ASCS facilitates reliable data collection from sensors, efficient routing through the mesh, and seamless integration with backend infrastructure via MQTT.

This plugin is designed as a foundational component for critical city services, emphasizing reliability, security, and clear operational procedures.

**[Project Status: Development / Beta / Stable]** 

---

**Table of Contents:**

* [Features](#features)
* [Architecture Overview](#architecture-overview)
* [Requirements](#requirements)
* [Installation & Setup (PlatformIO)](#installation--setup-platformio)
* [Configuration](#configuration)
* [Usage & Node Roles](#usage--node-roles)
* [MQTT Integration Details](#mqtt-integration-details)
* [Reliability & Scalability](#reliability--scalability)
* [Security Considerations](#security-considerations)
* [Troubleshooting](#troubleshooting)
* [Contributing](#contributing)
* [License](#license)

---

## Features

* **Modular Node Roles:** Supports distinct, configurable node functions:
    * **Sensor:** Collects data from attached sensors and transmits it efficiently.
    * **Aggregator (Optional):** Relays sensor data towards gateways (simple forwarding implemented).
    * **Gateway:** Bridges the Meshtastic LoRa mesh network to standard IP networks, forwarding data securely to MQTT brokers.
* **Service Discovery:** Nodes periodically announce their role, enabling dynamic network topology awareness, particularly for locating active gateways.
* **Flexible & Efficient Sensor Data:** Utilizes Protocol Buffers (`proto/SmartCity.proto`) for structured, compact data serialization. Supports diverse sensor readings via a `map<string, float>` field.
* **Robust MQTT Integration:** Gateways dynamically format received sensor data into JSON and publish to configurable, structured MQTT topics. Includes basic reconnection logic and message buffering.
* **Persistent Configuration:** Leverages the ESP32 `Preferences` library (NVS) for reliable storage of node role, network parameters, credentials, and operational settings across reboots.
* **PlatformIO Focused:** Designed for integration within the PlatformIO ecosystem for streamlined development, dependency management, and building.
* **Abstract Sensor Interface:** Simplifies adding support for new physical sensor hardware (`src/interfaces/SensorInterface.h`).
* **Gateway Message Buffering:** Basic filesystem buffering (SPIFFS/LittleFS) on Gateways to handle temporary MQTT/WiFi disconnections.

## Architecture Overview

ASCS operates on a Meshtastic LoRa mesh network. Sensor nodes collect data and send it (potentially via Aggregators) to Gateway nodes. Gateways connect to an IP network (WiFi) and forward the data to an MQTT broker. Backend systems subscribe to the MQTT broker to receive and process the city service data.

+-------------+   LoRa Mesh   +--------------+   LoRa Mesh   +-------------+      IP Network      +--------------+      IP Network      +--------------------+| Sensor Node | <-----------> | Aggregator   | <-----------> | Gateway     | <------------------> | MQTT Broker  | <------------------> | Backend Services   || (Role 1)    |   (Packet)    | Node (Opt.)  |   (Packet)    | Node        |      (MQTT JSON)     |              |      (DB, API)       | (Database, Dashbd) |+-------------+               | (Role 2)     |               | (Role 3)    |                      +--------------+                      +--------------------++--------------+               +-------------+
*For a more detailed explanation, see [docs/architecture.md](docs/architecture.md).*
*For details on the packet structure, see [docs/packet_format.md](docs/packet_format.md).*

## Requirements

Successful deployment requires specific hardware, software, libraries, and infrastructure.

* **Hardware:** ESP32-based LoRa boards (recommended, especially for Gateways), compatible sensors.
* **Development:** PlatformIO IDE, Git, Python 3.x, Nanopb generator (`pip install nanopb`).
* **Libraries:** Meshtastic-device, Nanopb runtime, PubSubClient, ArduinoJson (see `requirements.md` or `platformio.ini` examples for specific versions).
* **Infrastructure:** WiFi network (for Gateways), secure MQTT Broker, backend processing systems.

*For a complete list, see [requirements.md](requirements.md).*

## Installation & Setup (PlatformIO)

This plugin is designed to be included in a PlatformIO project that builds the full Meshtastic firmware for your target device.

1.  **Clone Repository:** Obtain the ASCS project code.
2.  **Project Setup:** Create/use a PlatformIO project for your Meshtastic device.
3.  **Integrate ASCS Code:**
    * Copy the ASCS `src/` contents (or relevant `.h`/`.cpp` files like `AkitaSmartCityServices.h/.cpp`, `ASCSConfig.h/.cpp`, `interfaces/`) into your PlatformIO project's `src/` or `lib/ASCS/`.
    * Copy the `proto/` directory into your project.
4.  **Generate Protobuf Code:**
    * Run the Nanopb generator script on `proto/SmartCity.proto` using the `proto/SmartCity.options` file. This creates `SmartCity.pb.c` and `SmartCity.pb.h`.
        ```bash
        # Example (adjust paths)
        python <path_to_nanopb>/nanopb_generator.py proto/SmartCity.proto --options-file=proto/SmartCity.options --output-dir=src/generated_proto
        ```
    * Ensure the generated files are compiled by PlatformIO (usually automatic for `.c` files in `src`).
    * **Crucial:** The `.options` file links the C++ map callback functions. Ensure callbacks in `AkitaSmartCityServices.cpp` are correctly implemented.
5.  **Configure `platformio.ini`:**
    * Add required `lib_deps` (Meshtastic, Nanopb, PubSubClient, ArduinoJson, sensor libraries).
    * Add the `-D ASCS_ROLE_GATEWAY` build flag if compiling for a Gateway.
    * Set board-specific flags (e.g., `-D HELTEC_LORA_V2`).
    * Configure filesystem (`board_build.filesystem = spiffs` or `littlefs`) if building a Gateway.
6.  **Integrate into `main.cpp`:**
    * Include headers (`Meshtastic.h`, `AkitaSmartCityServices.h`, your `SensorInterface` implementation).
    * Instantiate `AkitaSmartCityServices ascsPlugin;`.
    * Instantiate your sensor implementation (`std::unique_ptr<SensorInterface> ...`).
    * **Before `meshtastic.begin()`:**
        * `ascsPlugin.setSensor(std::move(mySensor));` (if applicable)
        * `meshtastic.addPlugin(&ascsPlugin);`
    * Initialize Filesystem (`FileSystem.begin()`) if building a Gateway.
    * Call `meshtastic.begin()` and `meshtastic.loop()`.
7.  **Flash Firmware:** Build and upload using PlatformIO.

*See example projects in `examples/` for concrete implementations.*
*For detailed steps, see [docs/deployment_guide.md](docs/deployment_guide.md).*

## Configuration

ASCS nodes **must** be configured after flashing using the Meshtastic `Preferences` system (namespace: `ascs`). Use the Serial Console (`!prefs set <key> <value>`) or Python API (`meshtastic --setprefs <key> <value>`).

**Key Parameters:**

* `role` (uint): `1`=Sensor, `2`=Aggregator, `3`=Gateway **(Required)**
* `wifi_ssid`, `wifi_pass` (string): **(Required for Gateway)**
* `mqtt_srv`, `mqtt_port`, `mqtt_user`, `mqtt_pass`, `mqtt_topic` (string/int): **(Required for Gateway)**
* Other parameters: `service_id`, `target_node`, `read_int`, `disc_int`, `svc_tout`, `mqtt_rec_int`.

**Remember to use `!prefs commit` and `!reboot` after setting values via serial.**

*For a full list of keys, defaults, and examples, see [docs/configuration.md](docs/configuration.md).*

## Usage & Node Roles

* **Sensor:** Reads data via its `SensorInterface` implementation at the `read_int` interval. Formats and sends `SensorData` packets towards a configured `target_node` or discovered Gateway. Broadcasts `ServiceDiscovery`.
* **Aggregator:** Listens for `SensorData`. Forwards received packets towards a configured `target_node` or discovered Gateway. Broadcasts `ServiceDiscovery`.
* **Gateway:** Listens for `SensorData`. Connects to WiFi and MQTT. Publishes received data as JSON to MQTT or buffers it to the filesystem if disconnected. Broadcasts `ServiceDiscovery`. Processes the buffer upon reconnection.

## MQTT Integration Details

* **Topic Structure:** Gateways publish to:
    `<mqtt_base_topic>/sensor/<gateway_service_id>/<originating_node_id_hex>/<sensor_id>`
    * Example: `akita/smartcity/sensor/99/a1b2c3d4/BME280-Floor1`
* **Payload Format:** JSON object containing `node_id`, `sensor_id`, `timestamp_utc`, `sequence_num`, and a nested `readings` object mirroring the `map<string, float>` from the `SensorData` packet.
    ```json
    {
      "node_id": "a1b2c3d4",
      "sensor_id": "BME280-Floor1",
      "timestamp_utc": 1714148000,
      "sequence_num": 123,
      "readings": {
        "temperature_c": 22.5,
        "humidity_pct": 45.8,
        "pressure_pa": 101325.0
      }
    }
    ```
    *(**Note:** Correct population of the `readings` object requires passing decoded map data through the gateway's processing functions - requires minor refactoring noted in code comments).*

*See [docs/packet_format.md](docs/packet_format.md) for more on data structures.*
*Use the [tools/mqtt_test_subscriber.py](tools/mqtt_test_subscriber.py) script for testing.*

## Reliability & Scalability

* **Protocol Buffers:** Ensure efficient use of LoRa airtime.
* **Gateway Buffering:** Handles temporary network outages. Buffer size and management strategy may need tuning.
* **Service Discovery:** Allows dynamic adaptation to network changes.
* **Meshtastic Limits:** Be mindful of LoRa duty cycles, packet size limits, and practical mesh size constraints. Optimize sensor reporting intervals (`read_int`).
* **Future Work:** Application-level ACKs, more sophisticated Aggregator logic (buffering/aggregation), and advanced buffer management could further enhance reliability for critical data.

## Security Considerations

* **Meshtastic Channel:** **Use a private channel with a strong Pre-Shared Key (PSK)** to encrypt LoRa traffic. Configure via Meshtastic settings (`!meshtastic --setchan ...`).
* **MQTT Security:** **Configure your MQTT broker for TLS encryption and strong authentication.** Use the `mqtt_user` and `mqtt_pass` settings. Protect these credentials.
* **WiFi Security:** Use WPA2/WPA3 for Gateway WiFi connections.
* **Physical Security:** Protect deployed nodes from tampering.
* **Input Validation:** Sanitize any potential downlink commands received via MQTT before acting on them (future feature).

## Troubleshooting

1.  **Check Serial Logs:** Essential for diagnosing boot, connection, sensor, and plugin errors.
2.  **Verify Configuration:** Use `!prefs list ascs` and `!meshtastic --info`.
3.  **Check Network Path:** Verify Meshtastic connectivity, Gateway WiFi connection, MQTT broker status, and MQTT subscriptions.
4.  **Isolate Components:** Test sensor functionality, mesh communication, and gateway publishing independently if possible.

*For common issues and solutions, see [docs/troubleshooting.md](docs/troubleshooting.md).*

## Contributing

Contributions are welcome to improve ASCS for Akita's smart city needs! Focus areas include:

* Implementing and testing Nanopb map callbacks thoroughly.
* Refining Gateway buffering and MQTT publishing logic (passing map data).
* Adding robust unit and integration tests (see [tests/README.md](tests/README.md)).
* Developing more sophisticated Aggregator features.
* Implementing remote configuration and downlink commands.
* Adding more sensor examples and documentation.

Please follow standard Git workflow (fork, branch, pull request) and provide clear descriptions and testing for changes. Report bugs or suggest features via the project's issue tracker.


---
