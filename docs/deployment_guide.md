# Akita Smart City Services (ASCS) - Deployment Guide

This guide provides practical steps for deploying ASCS nodes (Sensors, Gateways) within a Meshtastic network.

## Prerequisites

1.  **Compiled Firmware:** You need firmware binaries compiled using PlatformIO for your specific target hardware (e.g., ESP32, nRF52) with the ASCS plugin included.
    * Ensure the correct build flags (`-D ASCS_ROLE_GATEWAY` if applicable) were used.
    * Ensure Nanopb code generation (`.pb.c`/`.pb.h`) was successful and included.
2.  **Hardware:** Assembled and tested Meshtastic nodes (Sensor hardware attached if applicable).
3.  **Configuration Plan:** Know the intended role (`SENSOR`, `GATEWAY`), `service_id`, network credentials (WiFi/MQTT for Gateways), and desired intervals for each node being deployed.
4.  **Tools:**
    * PlatformIO CLI or IDE for flashing firmware.
    * USB cable for connecting to nodes.
    * Serial terminal application (e.g., PlatformIO Serial Monitor, PuTTY) OR Meshtastic Python API for configuration.
5.  **Infrastructure:** Operational WiFi network and MQTT broker accessible to planned Gateway locations.

## Deployment Steps

**Step 1: Flash Firmware**

1.  Connect the Meshtastic node to your computer via USB.
2.  Identify the correct serial port for the device.
3.  Use PlatformIO to upload the compiled firmware binary to the device.
    * **PlatformIO IDE:** Use the "Upload" button/task for the correct environment.
    * **PlatformIO CLI:** `pio run -t upload --environment <your_env_name>`
4.  Wait for the flashing process to complete. The device will likely reboot.

**Step 2: Initial Connection & Meshtastic Configuration**

1.  Connect to the device's serial console (e.g., `pio device monitor`).
2.  Perform basic Meshtastic configuration if this is a new device or needs changes:
    * Set Region: `!meshtastic --set region US` (or `EU433`, `JP`, etc.)
    * Set LoRa Channel: It is **highly recommended** to use a private channel with a Pre-Shared Key (PSK) for security.
        * Generate a new channel URL: `!meshtastic --ch-set psk random --ch-index 1` (Sets channel 1 with a random PSK)
        * OR set specific parameters: `!meshtastic --setchan psk 0x<your_secret_key_in_hex> --ch-index 1`
        * Ensure all ASCS nodes use the *same* channel settings.
    * (Optional) Set Node Name/Alias: `!meshtastic --set name MySensor-01`

**Step 3: ASCS Plugin Configuration**

1.  While connected to the serial console, use `!prefs` commands to configure the ASCS plugin parameters stored under the `ascs` namespace.
2.  **Refer to `docs/configuration.md` for all keys and their meanings.**
3.  **Example for a SENSOR Node:**
    ```bash
    !prefs set role 1
    !prefs set service_id 101
    !prefs set read_int 300000 # 5 minutes
    !prefs set disc_int 600000 # 10 minutes
    !prefs set svc_tout 1800000 # 30 minutes
    # target_node can often be left at 0 to auto-discover gateways
    ```
4.  **Example for a GATEWAY Node:**
    ```bash
    !prefs set role 3
    !prefs set service_id 99 # Gateway service group
    !prefs set disc_int 300000 # 5 minutes
    !prefs set svc_tout 900000 # 15 minutes
    !prefs set wifi_ssid YourCityWiFi_SSID
    !prefs set wifi_pass YourCityWiFi_Password
    !prefs set mqtt_srv mqtt.city.domain.com
    !prefs set mqtt_port 1883
    !prefs set mqtt_user gateway_user_01
    !prefs set mqtt_pass S3cr3tMQTTpassW0rd
    !prefs set mqtt_topic city/iot/prod/ascs
    !prefs set mqtt_rec_int 15000 # 15 seconds
    ```
5.  **Verify Settings:** Use `!prefs list` to check the configured values.
6.  **Commit Changes:** **Crucial:** Save the settings permanently: `!prefs commit`
7.  **Reboot:** Restart the node to apply all settings: `!reboot`

**Step 4: Verify Operation**

1.  **Serial Logs:** Monitor the serial output after reboot.
    * Check for ASCS plugin initialization messages.
    * **Sensor:** Look for sensor reading messages and packet sending confirmations.
    * **Gateway:** Look for WiFi connection attempts/success, MQTT connection attempts/success, and messages indicating received packets being published or buffered.
2.  **Meshtastic Tools:** Use the Meshtastic App or Python API to check if the node appears in the node list and if other nodes can see it.
3.  **MQTT Broker:** Use an MQTT client (like `mosquitto_sub`, MQTT Explorer) to subscribe to the configured topic (`<mqtt_base_topic>/#`) and verify that data from Sensor nodes is arriving via the Gateway. Check the JSON payload structure.

**Step 5: Physical Installation**

1.  **Location:** Choose appropriate locations based on node role.
    * **Sensors:** Near the asset being monitored. Consider LoRa range to potential Aggregators/Gateways.
    * **Gateways:** Locations with reliable power and good WiFi coverage. Placement might affect LoRa mesh connectivity.
2.  **Enclosure:** Use weatherproof enclosures suitable for outdoor deployment.
3.  **Antenna:** Ensure LoRa antennas are properly connected and oriented for best performance. Use appropriate antenna types for the frequency band.
4.  **Power:** Provide a reliable power source (USB adapter, battery with solar charging). Monitor battery levels if applicable (consider adding battery voltage to `SensorData`).

## Ongoing Maintenance

* Monitor node status via Meshtastic tools or backend data analysis (e.g., check for gaps in sensor readings).
* Monitor Gateway connectivity (WiFi, MQTT).
* Update firmware periodically with Meshtastic and ASCS plugin improvements.
* Check physical condition of deployed nodes and enclosures.
