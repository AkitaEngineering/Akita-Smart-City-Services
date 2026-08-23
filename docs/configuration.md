# Device configuration

ASCS reads the ESP32 NVS namespace `ascs`. The official Meshtastic console does not provide arbitrary ASCS NVS writes, so use the included one-time provisioning image.

1. Copy `provisioning/esp32_nvs/include/provisioning.example.h` to `provisioning.h` in the same directory.
2. Set role (`1` sensor, `2` aggregator, `3` gateway), unique nonzero service ID, intervals, and gateway secrets. Put the complete PEM root/intermediate CA chain in `ASCS_PROVISION_MQTT_CA_CERT` using concatenated C string literals.
3. Change the PlatformIO board in `provisioning/esp32_nvs/platformio.ini` to the exact ESP32 target when it is not Heltec WiFi LoRa 32 V2.
4. Run `pio run -d provisioning/esp32_nvs -t upload`, monitor at 115200 baud, and require the success message.
5. Flash the ASCS Meshtastic image without an erase operation. A full flash erase also removes NVS and requires reprovisioning.

The untracked `provisioning.h` contains secrets and must never be committed or archived with build logs.

| NVS key | Constraint |
| --- | --- |
| `role` | 1–3 |
| `service_id` | nonzero |
| `target_node` | uint32 mesh node, 0 for discovery |
| `trusted_gw` | exact nonzero gateway node ID required on actuator nodes |
| `read_int`, `disc_int` | at least 1000 ms |
| `svc_tout` | greater than discovery interval |
| `mqtt_rec_int` | at least 1000 ms |
| `wifi_ssid`, `wifi_pass` | gateway network; at most 32/64 bytes |
| `mqtt_srv`, `mqtt_port` | TLS broker hostname (at most 253 bytes, no whitespace or path separators) and port; default 8883 |
| `mqtt_user`, `mqtt_pass` | required gateway credential; at most 256/4096 bytes |
| `mqtt_topic` | concrete base topic up to 128 bytes with no wildcards, whitespace, empty levels, or leading/trailing slash; normally `akita/smartcity` |
| `mqtt_ca` | required PEM trust anchor; at most 8192 bytes |

Firmware validation is fail closed: invalid sensor hardware or gateway configuration prevents ASCS initialization. Use a strong private Meshtastic channel PSK and the correct legal radio region through supported Meshtastic configuration tools.
