# Akita Smart City Services (ASCS) - Configuration Guide

This document details the configuration parameters used by the ASCS plugin. These settings are stored persistently in the device's Non-Volatile Storage (NVS) using the ESP32 `Preferences` library under the namespace `"ascs"`.

Configuration is typically performed **after** flashing the firmware using the Meshtastic Serial Console or the Meshtastic Python API.

**Configuration Namespace:** `ascs`

## Configuration Keys

| Key           | Type   | Default Value (from ASCSConfig.h) | Role(s) Affected | Description                                                                                                                               | Example Serial Command (`!prefs set <key> <value>`) |
|---------------|--------|-----------------------------------|------------------|-------------------------------------------------------------------------------------------------------------------------------------------|---------------------------------------------------|
| `role`        | uint   | `1` (SENSOR)                      | All              | Sets the primary function of the node. `1`: Sensor, `2`: Aggregator, `3`: Gateway. **Required.** | `!prefs set role 3` (Set as Gateway)              |
| `service_id`  | uint   | `1`                               | All              | Logical identifier for a group, location, or specific service. Can be used for MQTT topic structure or filtering.                           | `!prefs set service_id 101`                       |
| `target_node` | uint   | `0`                               | Sensor, Aggregator| Preferred destination Node ID (Hex format, e.g., `0xa1b2c3d4`) for Sensor/Aggregator data. `0` means auto-discover Gateway or broadcast. | `!prefs set target_node 0xDEADBEEF`               |
| `read_int`    | uint   | `60000` (ms)                      | Sensor           | Interval (in milliseconds) at which the Sensor node reads data from its physical sensor(s).                                                | `!prefs set read_int 300000` (5 minutes)          |
| `disc_int`    | uint   | `300000` (ms)                     | All              | Interval (in milliseconds) at which the node broadcasts its Service Discovery message.                                                    | `!prefs set disc_int 600000` (10 minutes)         |
| `svc_tout`    | uint   | `900000` (ms)                     | All              | Timeout (in milliseconds) after which an inactive node is removed from the local service discovery table. Should be > `disc_int`.         | `!prefs set svc_tout 1800000` (30 minutes)        |
| `mqtt_rec_int`| uint   | `10000` (ms)                      | Gateway          | Interval (in milliseconds) between MQTT reconnection attempts if the connection is lost.                                                  | `!prefs set mqtt_rec_int 30000` (30 seconds)      |
| `wifi_ssid`   | string | `"YourWiFi_SSID"`                 | Gateway          | The SSID (name) of the WiFi network the Gateway should connect to. **Required for Gateway.** | `!prefs set wifi_ssid MyCityWiFi`                 |
| `wifi_pass`   | string | `"YourWiFiPassword"`              | Gateway          | The password for the WiFi network. **Required for Gateway.** | `!prefs set wifi_pass CityWiFiPa$$w0rd`           |
| `mqtt_srv`    | string | `"your_mqtt_broker.com"`          | Gateway          | The hostname or IP address of the MQTT broker. **Required for Gateway.** | `!prefs set mqtt_srv mqtt.akita.gov`              |
| `mqtt_port`   | int    | `1883`                            | Gateway          | The port number for the MQTT broker (typically 1883 for unencrypted, 8883 for TLS). **Required for Gateway.** | `!prefs set mqtt_port 1883`                       |
| `mqtt_user`   | string | `""` (empty)                      | Gateway          | The username for MQTT authentication. Leave empty if no authentication is used. **Required for Gateway.** | `!prefs set mqtt_user ascs_gateway_1`             |
| `mqtt_pass`   | string | `""` (empty)                      | Gateway          | The password for MQTT authentication. **Required for Gateway.** | `!prefs set mqtt_pass Sup3rS3cr3t!`               |
| `mqtt_topic`  | string | `"akita/smartcity"`               | Gateway          | The base topic string used for publishing MQTT messages. **Required for Gateway.** | `!prefs set mqtt_topic city/akita/iot/prod`       |

## Setting Configuration

**Method 1: Meshtastic Serial Console**

1.  Connect to the device's USB serial port using a terminal emulator (like PlatformIO Serial Monitor, PuTTY, screen).
2.  Use the `!prefs set <key> <value>` command for each parameter you want to change.
3.  Use `!prefs list` to view current settings (including the `ascs` namespace).
4.  **Important:** After setting all desired values, use `!prefs commit` to save changes to NVS.
5.  Use `!reboot` to restart the device with the new configuration.

**Method 2: Meshtastic Python API**

1.  Install the Meshtastic Python library (`pip install meshtastic`).
2.  Connect to the device (e.g., via Serial or TCP).
3.  Use the `meshtastic --setprefs <key> <value>` command-line tool or write a Python script using the `interface.setPrefs()` method.
    ```python
    import meshtastic
    import meshtastic.prefs

    # Example using Python API
    interface = meshtastic.StreamInterface() # Or SerialInterface()
    prefs = interface.localNode.getPrefs()

    # Set values (use correct preference level, e.g., EDITABLE)
    prefs.set('role', 3, meshtastic.prefs.WarningLevel.EDITABLE)
    prefs.set('wifi_ssid', 'MyCityWiFi', meshtastic.prefs.WarningLevel.EDITABLE)
    # ... set other preferences ...

    # Write changes back to device
    interface.localNode.setPrefs(prefs)

    # Optional: Reboot device
    # interface.localNode.reboot()
    interface.close()
    ```
4.  Ensure changes are saved (the API might handle commit implicitly, or you might need `interface.localNode.writePrefs()`).

## Default Values

If a key is not found in the `Preferences` storage, the default value defined in `ASCSConfig.h` will be used by the plugin during initialization. It's recommended to explicitly set all required parameters for production deployments.
