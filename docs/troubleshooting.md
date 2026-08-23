# Troubleshooting

- `Invalid configuration`: reprovision NVS; gateway requires WiFi, broker, username/password, port, and PEM CA. Sensor role requires a compiled and detected sensor implementation.
- TLS connection failure: check device time, broker hostname/SAN, full trust chain, expiry, port, ACL, and DNS. Do not disable certificate verification.
- No telemetry: verify matching private channel/region, discovery logs, sensor finite values, Meshtastic payload size, broker subscription, and scoped topic ACL.
- Queue does not drain: check LittleFS mount, MQTT connectivity, broker publish ACL, and logs for corrupt/incomplete queue records. The queue is FIFO and bounded; sustained outages eventually evict oldest telemetry.
- Command remains pending: confirm gateway control subscription, accurate gateway/device clocks, exact eight-hex asset/node ID, target actuator registration, private-channel gateway discovery, and acknowledgement ACL. The console waits 60 seconds.
- Firestore permission denied: force-refresh the user's ID token after assigning an `operator`/`admin` custom claim and verify the deployed rules/project.
- Official firmware compile fails outside `src/modules/ascs`: reproduce the same Meshtastic commit without ASCS. Board/toolchain failures in upstream firmware must be resolved or pinned before release.
