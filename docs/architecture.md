# Architecture

```text
BME280 / actuator
       │
       ▼
ASCS sensor ── encrypted Meshtastic private channel ──► ASCS gateway
       ▲                         │                          │
       └──── optional ASCS aggregator                       │ TLS MQTT
                                                          ▼
                                               MQTT broker + ACLs
                                                  │             │
                                        telemetry bridge      console
                                                  │             │
                                               Firestore ◄──────┘ audit
```

Every node periodically announces role and service ID. Sensors choose a configured target, a discovered gateway, or broadcast. Aggregators preserve the originating sensor node while decoding and re-encoding validated telemetry toward a gateway. Each receiver requires fresh same-service discovery for the origin and relay roles. Gateways publish immediately when connected and otherwise append bounded records to LittleFS; reconnect drains records in FIFO order.

Control travels in the reverse direction. The gateway accepts strict, short-lived JSON only on `<base>/control/<8-hex-node>`, translates it to Protobuf, and sends a unicast mesh packet with Meshtastic acknowledgement requested. A target accepts unexpired commands only from its provisioned trusted gateway node after that node has announced the gateway role for the same service, and only when its concrete actuator owns the asset ID. Recently completed UUIDs are replayed from a bounded result cache instead of executing twice. The actuator result returns as `ControlAck`; the console correlates the acknowledgement topic, UUID, and node ID before changing the Firestore audit result.

Trust boundaries are the encrypted private LoRa channel, broker TLS/ACL, Firebase Authentication custom claims and signed ID tokens, Firestore rules, and physical device/NVS access. MQTT retained commands are disabled. A Meshtastic transport acknowledgement is not treated as actuator success.
