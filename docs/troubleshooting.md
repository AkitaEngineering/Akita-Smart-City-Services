# Akita Smart City Services (ASCS) - Troubleshooting Guide

This guide provides steps to diagnose common issues with ASCS nodes.

## General Approach

1.  **Check Power:** Ensure the node has a stable power source. Low battery can cause erratic behavior.
2.  **Check Serial Logs:** This is the most important tool. Connect via USB and monitor the serial output during boot and operation. Increase Meshtastic log verbosity if needed (`!meshtastic --set serial_debug_level DEBUG`). Look for error messages related to ASCS, Meshtastic, WiFi, MQTT, Sensors, Filesystem, or Nanopb.
3.  **Check Configuration:** Verify all Meshtastic and ASCS (`!prefs list`) settings are correct for the node's role and environment. Remember to `!prefs commit` and `!reboot` after changes.
4.  **Check Meshtastic Network:** Use the Meshtastic App, Web UI, or Python API to:
    * See if the node appears in the node list.
    * Check its last heard time.
    * Check its reported neighbors and metrics (RSSI, SNR).
    * Send test messages to/from the node.
5.  **Isolate the Problem:** Try to determine if the issue is with the Sensor, the Mesh communication, the Gateway's IP connection, MQTT, or the backend.

## Common Issues & Solutions

**Issue: Node Not Showing Up / Not Communicating on Mesh**

* **Cause:** Incorrect LoRa region/channel settings (PSK mismatch, frequency).
    * **Solution:** Verify region (`!meshtastic --info`) and channel settings (`!meshtastic --ch-index 0 --info`) match other nodes. Reset channel if unsure (`!meshtastic --ch-set psk random --ch-index 0`).
* **Cause:** Hardware issue (antenna disconnected, LoRa module failure).
    * **Solution:** Check antenna connection. Check serial logs for LoRa initialization errors. Try different hardware if possible.
* **Cause:** Firmware crash / boot loop.
    * **Solution:** Check serial logs during boot. Reflash firmware. If persistent, there might be a bug in the firmware or plugin.
* **Cause:** Power issue.
    * **Solution:** Check power supply/battery voltage.

**Issue: Sensor Node Not Sending Data (or Sending Infrequently)**

* **Cause:** Incorrect `role` setting (not set to 1).
    * **Solution:** Set `!prefs set role 1`, `!prefs commit`, `!reboot`.
* **Cause:** Sensor hardware failure or incorrect wiring.
    * **Solution:** Check sensor wiring. Check serial logs for sensor initialization or read errors from the `SensorInterface` implementation.
* **Cause:** `read_int` setting too high.
    * **Solution:** Check `!prefs list`. Set `!prefs set read_int <desired_ms>`.
* **Cause:** No `SensorInterface` implementation provided or `setSensor()` not called correctly in `main.cpp`.
    * **Solution:** Review `main.cpp` setup code. Check logs for warnings about missing sensor implementation.
* **Cause:** Nanopb encoding error (especially map callbacks).
    * **Solution:** Check serial logs for `pb_encode` errors. Ensure map callbacks are correctly implemented and linked via `.options` file.

**Issue: Gateway Not Connecting to WiFi**

* **Cause:** Incorrect `wifi_ssid` or `wifi_pass`.
    * **Solution:** Double-check credentials (`!prefs list`). Remember they are case-sensitive.
* **Cause:** Poor WiFi signal strength at Gateway location.
    * **Solution:** Test WiFi signal with another device. Relocate Gateway or improve WiFi coverage.
* **Cause:** WiFi network issue (DHCP problem, MAC filtering).
    * **Solution:** Check WiFi router logs/settings.
* **Cause:** ESP32 WiFi hardware issue.
    * **Solution:** Check serial logs for WiFi errors. Try different hardware.

**Issue: Gateway Not Connecting to MQTT Broker**

* **Cause:** Incorrect `mqtt_srv`, `mqtt_port`, `mqtt_user`, or `mqtt_pass`.
    * **Solution:** Verify all MQTT settings (`!prefs list`). Check case sensitivity. Ensure user/pass are correct if broker requires authentication.
* **Cause:** MQTT broker down or inaccessible from Gateway's network.
    * **Solution:** Verify broker status. Check firewall rules. Try connecting from another client on the same network as the Gateway.
* **Cause:** MQTT TLS/SSL issues (if broker requires encryption).
    * **Solution:** `PubSubClient` has limited built-in TLS support. May require using `WiFiClientSecure` and potentially managing certificates, which is currently **not implemented** in the ASCS example code. Check broker requirements.
* **Cause:** MQTT Client ID conflict (unlikely with current implementation using Node ID, but possible).
    * **Solution:** Check broker logs for connection rejections due to client ID clashes.
* **Cause:** `PubSubClient` buffer size too small (if payloads are large).
    * **Solution:** Increase buffer size via `m_mqttClient->setBufferSize()` before connecting (requires code change). Check logs for publish failures.

**Issue: Data Not Arriving at MQTT Broker (Gateway seems connected)**

* **Cause:** Incorrect MQTT base topic (`mqtt_topic`).
    * **Solution:** Verify the `mqtt_topic` setting. Ensure your MQTT test subscriber is using the correct wildcard topic (e.g., `city/iot/prod/ascs/#`).
* **Cause:** Gateway is buffering data due to intermittent MQTT publish failures.
    * **Solution:** Check serial logs for publish errors or messages about buffering. Check `PubSubClient` buffer size. Monitor MQTT connection stability. Check buffer file size on the Gateway (`/ascs_buffer.dat`).
* **Cause:** Error during JSON serialization (e.g., `StaticJsonDocument` too small).
    * **Solution:** Check serial logs for JSON errors or warnings about truncation/overflow. Increase `jsonCapacity` in `publishMqtt`.
* **Cause:** Error during Nanopb decoding on the Gateway (e.g., map callback failure).
    * **Solution:** Check serial logs for `pb_decode` errors when packets arrive.
* **Cause:** Packet loss on the LoRa mesh (sensor data never reaches Gateway).
    * **Solution:** Check Meshtastic node list/map. Improve antenna placement or add Aggregator nodes if necessary. Check RSSI/SNR values.

**Issue: Gateway Buffer File (`/ascs_buffer.dat`) Grows Very Large or Seems Corrupted**

* **Cause:** Prolonged MQTT disconnection prevents buffer clearing.
    * **Solution:** Resolve MQTT connectivity issue. The buffer should process automatically upon reconnection.
* **Cause:** Bug in buffer read/write/remove logic.
    * **Solution:** Review `bufferPacket`, `readPacketFromBuffer`, `removePacketFromBuffer` code. Check logs for file I/O errors.
* **Cause:** Corrupted data written due to crash or power loss during write.
    * **Solution:** Manually delete the buffer file via serial console or a custom command if implemented. Consider adding checks on boot to validate/clear the buffer file if it seems corrupted.

**Issue: Nanopb Encoding/Decoding Errors**

* **Cause:** Mismatch between `.proto` file used to generate code and the code being run.
    * **Solution:** Ensure `.pb.c`/`.pb.h` files are regenerated whenever `SmartCity.proto` changes. Clean and rebuild the project.
* **Cause:** Incorrect implementation of map callbacks (or missing `.options` file).
    * **Solution:** Carefully review `encode_map_callback` and `decode_map_callback` against Nanopb documentation. Ensure `SmartCity.options` exists and is correct.
* **Cause:** Buffer sizes too small (either the static buffer used for `pb_ostream_from_buffer` or internal nanopb limits).
    * **Solution:** Increase buffer sizes (e.g., `ASCS_GATEWAY_MAX_PACKET_SIZE`). Check nanopb documentation for limits.
* **Cause:** Corrupted packet received over LoRa.
    * **Solution:** Usually results in `pb_decode` errors. Improve mesh reliability if frequent.
