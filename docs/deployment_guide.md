# Production deployment

1. Check out the official Meshtastic firmware commit in `meshtastic.version` and record the board environment, ASCS commit, toolchain, protobuf schema, and generated binary hashes.
2. Install ASCS with `tools/install_meshtastic_module.py`; build the exact sensor/aggregator/gateway environments. Never ship an image produced for another board revision.
3. Provision NVS, configure the legal radio region and a strong private channel, flash without erasing NVS, and confirm clean boot logs.
4. Configure broker TLS, Firebase JWT validation, and claim-derived ACLs. Gateway and operator principals must be separate. Validate token issuer, audience, signature, expiry, and the `operator`/`admin` role claim. Disable anonymous access and retained control messages; monitor authentication failures and queue depth.
5. Configure Firebase Auth custom claims, deploy `firestore.rules`, configure the MQTT-to-Firestore bridge with Admin SDK credentials, build the console with production variables, and deploy hosting.
6. Run hardware-in-the-loop acceptance: known readings, invalid reading rejection, reboot persistence, WiFi/MQTT outage and FIFO recovery, queue-cap eviction, valid actuator execution, unauthorized sender rejection, malformed command rejection, target mismatch, and timeout.
7. Pilot in a controlled location. Verify RF coverage, duty cycle, clock quality, power budget, watchdog recovery, TLS expiry behavior, and environmental enclosure performance before fleet rollout.

Operate with staged firmware rollout and rollback images, asset/credential inventory, certificate and PSK rotation procedures, alerting for missing sequence numbers and stale nodes, Firestore retention/backups, broker metrics, and periodic physical inspection. A software build alone is not authorization to deploy safety-critical actuators; each actuator needs a hazard analysis and a local safe state.
