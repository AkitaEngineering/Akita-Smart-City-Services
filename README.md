# Akita Smart City Services (ASCS) - Meshtastic Plugin 

This Meshtastic plugin enables the creation of a distributed smart city IoT network using Meshtastic devices. It facilitates service discovery, flexible sensor data transmission using Protocol Buffers, and robust MQTT integration for gateway nodes. This is a redesigned version aiming for better structure, reliability, and maintainability.

**Key Features:**

* **Node Roles:** Supports distinct node functions:
    * **Sensor:** Collects data from attached sensors and transmits it.
    * **Aggregator (Optional):** Relays sensor data towards gateways (simple forwarding implemented).
    * **Gateway:** Bridges the Meshtastic network to an MQTT broker.
* **Service Discovery:** Nodes periodically announce their role, allowing other nodes (especially sensors and aggregators) to find gateways.
* **Flexible Sensor Data:** Uses Protocol Buffers (`SmartCity.proto`) with a `map<string, float>` for readings, allowing transmission of various sensor data types. (**Note:** Map encoding/decoding requires implementing nanopb callbacks - currently simplified in the example).
* **MQTT Integration:** Gateway nodes dynamically format received sensor data (including all key-value pairs from the map) into JSON and publish to configurable MQTT topics.
* **Configuration:** Uses the ESP32 `Preferences` library (NVS) for persistent storage of node role, IDs, intervals, and network credentials.
* **PlatformIO Structure:** Designed for integration into a PlatformIO build environment.
* **Abstract Sensor Interface:** Allows easy integration of different physical sensor types.

**Requirements:**

* PlatformIO IDE (Recommended for building)
* Meshtastic-device firmware library (ensure compatibility, e.g., `^2.x.x`)
* **Nanopb:** Protocol Buffer library for C. You need both the library and the `nanopb_generator.py` script.
* **PubSubClient:** For MQTT communication (Gateway role).
* **ArduinoJson:** For formatting MQTT payloads (Gateway role).
* WiFi library (built-in for ESP32).
* An MQTT broker (for Gateway nodes).
* Compatible Meshtastic hardware (ESP32, nRF52 based).

**Installation & Setup (PlatformIO):**

1.  **Project Setup:** Create a PlatformIO project for the Meshtastic firmware or your standalone application.
2.  **Add Plugin Files:** Copy `AkitaSmartCityServices.h`, `AkitaSmartCityServices.cpp`, and `SensorInterface.h` into your project's `src/` directory or a dedicated library folder (e.g., `lib/ASCS/`).
3.  **Add Proto File:** Place `SmartCity.proto` in your project (e.g., in a `proto/` directory).
4.  **Generate Protobuf Code:**
    * Install `nanopb` (e.g., `pip install nanopb`).
    * Use the `nanopb_generator.py` script to generate `SmartCity.pb.h` and `SmartCity.pb.c` from `SmartCity.proto`. Place the generated files into your `src/` directory or a subdirectory (e.g., `src/generated_proto/`).
        ```bash
        # Example command (run from project root, adjust paths as needed)
        python <path_to_nanopb_generator>/nanopb_generator.py proto/SmartCity.proto --output-dir=src/generated_proto
        ```
    * Ensure the generated `.c` file is compiled and the `.h` file is included in your project build (PlatformIO typically handles `.c` files in `src/`). You might need to adjust include paths in your `.cpp` files.
    * **Map Callbacks:** For full functionality of the `map<string, float> readings` field, you *must* define encoding and decoding callback functions as described in the nanopb documentation and reference them in a `.options` file alongside your `.proto` file. The current C++ code includes warnings where this is needed.
5.  **Include Libraries:** Add the required libraries (`PubSubClient`, `ArduinoJson`, `nanopb`) to your `platformio.ini` `lib_deps`.
    ```ini
    lib_deps =
        meshtastic/Meshtastic-device @ ^2.x.x  # Check for latest compatible version
        knolleary/PubSubClient @ ^2.8
        bblanchon/ArduinoJson @ ^6.19.4
        nanopb/nanopb @ ^0.4.7 # Or path to your nanopb library source
        # Add any libraries needed for your specific sensor
    ```
6.  **Enable Gateway Features (Optional):** If building for a Gateway node, add the following build flag to your `platformio.ini` to include WiFi and MQTT code:
    ```ini
    build_flags =
        -D ASCS_ROLE_GATEWAY
        # Add other project build flags here
    ```
7.  **Integrate Plugin:** In your main firmware `src/main.cpp` (or equivalent):
    * Include `AkitaSmartCityServices.h`.
    * Include the header for your concrete `SensorInterface` implementation.
    * Create an instance of `AkitaSmartCityServices`.
    * Create an instance of your sensor implementation (e.g., using `std::make_unique`).
    * **Before `meshtastic.begin()`:**
        * Call `ascsPlugin.setSensor(std::move(mySensor))` if the node might be a Sensor.
        * Call `meshtastic.addPlugin(&ascsPlugin)`.
    * Call `meshtastic.begin()` and `meshtastic.loop()` as usual. (See `Example main.cpp`).
8.  **Configure Node:**
    * **First Boot / Manual Config:** The plugin uses default values initially. You need to set the actual configuration parameters. The easiest way is often via the Meshtastic Serial Console or Python API *after* the first boot:
        * Use `meshtastic --setprefs <key> <value>` or interactively set preferences using the serial console (`!prefs set <key> <value>`).
    * **Configuration Keys (Preferences Namespace: "ascs"):**
        * `role` (uint): `1`=Sensor, `2`=Aggregator, `3`=Gateway
        * `service_id` (uint): Service group ID (default: 1)
        * `target_node` (uint): Preferred NodeID (hex) for sending data (0 = auto/broadcast)
        * `read_int` (uint): Sensor read interval ms (default: 60000)
        * `disc_int` (uint): Discovery broadcast interval ms (default: 300000)
        * `svc_tout` (uint): Service timeout ms (default: 900000)
        * `wifi_ssid` (string): WiFi SSID (Gateway only)
        * `wifi_pass` (string): WiFi Password (Gateway only)
        * `mqtt_srv` (string): MQTT Broker address (Gateway only)
        * `mqtt_port` (int): MQTT Broker port (Gateway only)
        * `mqtt_user` (string): MQTT Username (Gateway only)
        * `mqtt_pass` (string): MQTT Password (Gateway only)
        * `mqtt_topic` (string): MQTT Base topic (Gateway only, default: "akita/smartcity")
    * **Example Serial Config:**
        ```
        # Set node role to Gateway
        !prefs set role 3
        # Set WiFi SSID
        !prefs set wifi_ssid MyNetworkSSID
        # Set MQTT Server
        !prefs set mqtt_srv mybroker.local
        # ... set other relevant prefs ...
        # Save and reboot
        !prefs commit
        !reboot
        ```
9.  **Build & Upload:** Compile and upload the firmware using PlatformIO.

**MQTT Topics (Gateway):**

Gateway nodes publish sensor data to MQTT topics dynamically constructed as:
`<mqtt_base_topic>/<role>/<service_id>/<from_node_id_hex>/<sensor_id>`

* `<mqtt_base_topic>`: Configured in preferences (default: `akita/smartcity`).
* `<role>`: Currently hardcoded to `sensor` (as data originates from sensors).
* `<service_id>`: The `service_id` configured *on the Gateway node*.
* `<from_node_id_hex>`: The Meshtastic Node ID (in hex) of the node that sent the `SensorData` packet.
* `<sensor_id>`: The `sensor_id` field from the `SensorData` packet (if present).

**Example Topic:** `akita/smartcity/sensor/1/a1b2c3d4/BME280-Floor1`

**MQTT Payload (Gateway):**

The payload is a JSON object containing:

```json
{
  "node_id": "a1b2c3d4", // Originating node ID (hex)
  "sensor_id": "BME280-Floor1", // Sensor ID from packet
  "timestamp_utc": 1714148000, // Unix timestamp from packet
  "sequence_num": 123, // Sequence number from packet
  "readings": {
    // Key-value pairs from the SensorData.readings map
    "temperature_c": 22.5,
    "humidity_pct": 45.8,
    "pressure_pa": 101325.0,
    "battery_v": 3.85
    // ... other readings ...
    // "status": "Map data unavailable" // If map decoding fails/not implemented
  }
  // Optional Meshtastic metadata like RSSI/SNR could be added here
}
```
Contributing:Contributions (pull requests, bug reports, feature suggestions) are welcome!
