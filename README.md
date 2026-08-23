# Akita Smart City Services

ASCS is a production-oriented private Meshtastic application for municipal telemetry and authenticated operator control. It contains an ESP32 module for current Meshtastic firmware, a TLS MQTT gateway, a Firebase-backed React operations console, generated Nanopb protocol code, and repeatable installation/provisioning tools.

## Implemented paths

- Sensor → Meshtastic private application port → optional aggregator → gateway → MQTT JSON.
- Operator → Firebase audit record → MQTT command → gateway → target actuator → device acknowledgement → console result.
- Gateway outage queue in LittleFS, bounded to 20 KiB with oldest-record eviction and corruption recovery.
- TLS server verification, MQTT authentication, private-channel sender discovery, strict packet/topic/schema bounds, operator/admin Firebase claims, and Firestore deny-by-default rules.
- BME280 sensor selected with `--sensor bme280`; other hardware is added through `SensorInterface` or `ActuatorInterface` and must implement its real device behavior.

ASCS uses Meshtastic `PRIVATE_APP` port 256. Reserve that port for ASCS on the deployed private mesh. A public Meshtastic port assignment is required before interoperating on networks where another private application may use the same port.

## Firmware build

Use the official [Meshtastic firmware](https://github.com/meshtastic/firmware) commit recorded in `meshtastic.version`. CI builds every ASCS role against that exact revision. ASCS deliberately does not include a fake radio implementation or a standalone substitute firmware.

```bash
python3 tools/install_meshtastic_module.py ../firmware \
  --base-env tbeam --role gateway
cd ../firmware
pio run -e ascs-tbeam-gateway
```

For a BME280 sensor:

```bash
python3 tools/install_meshtastic_module.py ../firmware \
  --base-env tbeam --role sensor --sensor bme280
cd ../firmware
pio run -e ascs-tbeam-sensor
```

The installer verifies the firmware checkout against `meshtastic.version`, copies `src/`, registers the module behind `ASCS_OFFICIAL_FIRMWARE`, and retains one ASCS PlatformIO environment per selected board and role. Re-running it is idempotent. `--allow-unpinned` exists only for deliberate development experiments. Update `meshtastic.version` and the matching CI checkout only after all role builds and hardware acceptance pass on a deliberate Meshtastic upgrade.

Provision NVS before production firmware by following [configuration.md](docs/configuration.md). Provisioning read-verifies every value and writes its schema marker last; production firmware rejects incomplete, obsolete, or role-mismatched configuration. Do not erase flash when replacing the provisioning image with the production image.

## Console checks and deployment

```bash
cd management-console
npm ci
npm run check
npm audit --audit-level=high
```

Copy `.env.example` to `.env`, supply the production Firebase project and MQTT-over-WebSocket endpoint, then build. Production rejects non-TLS MQTT URLs. Deploy `firestore.rules` and hosting with Firebase CLI from the repository root.

The console authenticates to MQTT with the signed Firebase ID token of the logged-in operator; no reusable MQTT password is embedded in the browser bundle. Configure the broker's JWT authentication to validate the Firebase issuer, audience, signature, expiry, and `role` custom claim. ACLs derived from that identity must permit reads only from `<base>/sensor/#` and `<base>/control/ack/#`, and publishes only to `<base>/control/+`. Gateway credentials are separate and may publish sensor/ack topics and subscribe only to control topics.

## Release gate

- `npm run check` and `npm audit --audit-level=high` pass.
- `python3 -m unittest discover -s tests -p 'test_*.py'` passes.
- Each selected official Meshtastic environment builds after installation.
- The NVS provisioning image reports success and production boot logs show ASCS initialized.
- Hardware-in-the-loop verifies sensor telemetry, network loss/recovery, command execution, rejection, timeout, and device acknowledgement.
- Broker ACLs, Firebase custom claims/rules, TLS certificate rotation, alerting, backups, regional LoRa settings, private channel PSK, enclosure, antenna, and power are reviewed.

See [architecture.md](docs/architecture.md), [packet_format.md](docs/packet_format.md), and [deployment_guide.md](docs/deployment_guide.md).
