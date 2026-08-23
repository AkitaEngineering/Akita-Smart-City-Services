# Requirements

- A pinned checkout of official Meshtastic firmware and PlatformIO Core compatible with that checkout.
- An ESP32 Meshtastic target. Gateway targets require supported WiFi and LittleFS.
- BME280 hardware for the included sensor build, or a completed concrete `SensorInterface` implementation.
- A completed, fail-safe concrete `ActuatorInterface` for every remotely controlled asset.
- A private Meshtastic channel, legal region configuration, and exclusive reservation of private application port 256.
- A TLS MQTT broker with DNS, trusted CA chain, Firebase JWT validation for operators, separate gateway credentials, claim-derived ACLs, monitoring, and retention disabled on control topics.
- Firebase Authentication, Firestore, Hosting, a trusted MQTT-to-Firestore bridge, and an Admin SDK process for assigning operator roles.
- Node.js 24 and npm for the console; Python 3 for the installer; Nanopb 0.4.9.1 only when regenerating protocol sources.
- Hardware-in-the-loop equipment and operational procedures listed in `docs/deployment_guide.md`.
