# Verification

The management console unit tests cover live and stored telemetry schema, topic identity, bounds, deduplication, and command topic/envelope/ack validation. Run `npm test` without deployment variables for unit tests. Run `npm run check` with the production variables from `.env.example` to include the fail-fast production build. CI also runs `npm run test:rules` inside the Firestore emulator to verify authorization, immutable audit fields, bounded command expiry/value, server timestamps, and denied telemetry writes.

Firmware compile verification is performed against a pinned official Meshtastic checkout:

```bash
python3 tools/install_meshtastic_module.py ../firmware --base-env tbeam --role gateway
pio run -d ../firmware -e ascs-tbeam-gateway
```

Production release additionally requires the hardware-in-the-loop cases in `docs/deployment_guide.md`; radio, I2C, flash wear/failure, broker ACL, and physical actuator behavior cannot be established by host unit tests.
