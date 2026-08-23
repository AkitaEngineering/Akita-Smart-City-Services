# Wire and MQTT protocol

`proto/SmartCity.proto` is the source of truth. Generated Nanopb files are committed under `src/generated_proto`. ASCS uses Meshtastic `PRIVATE_APP` port 256 and rejects encoded payloads above Meshtastic's data-payload limit.

`SmartCityPacket` contains exactly one of:

- `ServiceDiscovery`: role and service ID.
- `SensorData`: sensor ID (1–64 ASCII letters, digits, `.`, `_`, or `-`), Unix seconds, a non-empty map of up to 64 finite numeric readings with identifiers under the same rules, sequence number, and the nonzero originating mesh node ID.
- `ControlCommand`: UUID, uint32 target, asset ID, action, a bool or float value, and a Unix expiry time.
- `ControlAck`: UUID, `REJECTED`/`EXECUTED`/`FAILED`, and detail up to 96 characters.

Telemetry topic:

```text
<base>/sensor/<gateway-service-id>/<8-hex-origin-node>/<sensor-id>
```

Telemetry payload:

```json
{"node_id":"a1b2c3d4","sensor_id":"BME280","timestamp_utc":1714148000,"sequence_num":123,"readings":{"temperature_c":22.5}}
```

Sensors set `origin_node` on the mesh packet. Aggregators preserve it, and gateways accept a relay only when both the origin and immediate sender have recently advertised the expected sensor/aggregator roles for the same service. The MQTT `node_id` is always the validated origin, never the relay.

Command topic and payload:

```text
<base>/control/a1b2c3d4
```

```json
{"commandId":"550e8400-e29b-41d4-a716-446655440000","assetId":"a1b2c3d4","action":"power","value":true,"requestedAt":"2026-08-23T20:00:00.000Z","expiresAtUtc":1787515260,"requestedBy":"firebase-uid"}
```

Acknowledgement topic is `<base>/control/ack/<commandId>` with JSON fields `commandId`, `nodeId`, `status`, and `detail`. Topics and identifiers reject wildcards/path separators. The console grants commands 60 seconds; gateways reject already-expired or implausibly far-future commands, and devices fail closed if their trusted clock is unavailable or the mesh command expires. Commands and acknowledgements are not retained.

The console classifies canonical infrastructure sensor IDs `street_light`, `park_monitor`, `pool_system`, and `people_counter` as infrastructure. Custom infrastructure, fleet, and gateway identifiers use the prefixes `infra.`, `fleet.`, and `gateway.` respectively. All other sensor IDs are environmental unless a trusted Firestore bridge supplies an explicit category.
