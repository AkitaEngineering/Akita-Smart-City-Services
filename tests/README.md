# Verification

The management console unit tests cover telemetry schema/bounds/deduplication and command topic/envelope/ack validation. Run `npm run check` in `management-console`.

Firmware compile verification is performed against a pinned official Meshtastic checkout:

```bash
python3 tools/install_meshtastic_module.py ../firmware --base-env tbeam --role gateway
pio run -d ../firmware -e ascs-tbeam-gateway
```

Production release additionally requires the hardware-in-the-loop cases in `docs/deployment_guide.md`; radio, I2C, flash wear/failure, broker ACL, and physical actuator behavior cannot be established by host unit tests.
