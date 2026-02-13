# Akita Smart City Services (ASCS) - Packet Format

This document describes the Protocol Buffer message formats used for ASCS communication over the Meshtastic network.

## Overview

ASCS uses Protocol Buffers (protobuf) version 3 for efficient, structured data serialization. Messages are defined in `proto/SmartCity.proto` and compiled to C code using Nanopb.

All ASCS packets use the Meshtastic port number defined as `ASCS_PORT_NUM` (PortNum_APP_CUSTOM_MIN + 1).

## Message Structure

### SmartCityPacket

The root message wrapper containing all ASCS communication:

```protobuf
message SmartCityPacket {
  oneof payload {
    ServiceDiscovery discovery = 1;
    SensorData sensor_data = 2;
  }
}
```

### ServiceDiscovery

Used for nodes to announce their roles and capabilities:

```protobuf
message ServiceDiscovery {
  enum Role {
    UNKNOWN = 0;
    SENSOR = 1;
    AGGREGATOR = 2;
    GATEWAY = 3;
  }
  Role node_role = 1;
  uint32 service_id = 2;
}
```

### SensorData

Contains sensor readings and metadata:

```protobuf
message SensorData {
  string sensor_id = 1;
  uint32 timestamp_utc = 2;
  map<string, float> readings = 3;
  uint32 sequence_num = 4;
}
```

## Field Descriptions

### ServiceDiscovery
- `node_role`: The node's function in the ASCS network
- `service_id`: Optional identifier for grouping or location

### SensorData
- `sensor_id`: Optional string identifier for the sensor (e.g., "BME280-LivingRoom")
- `timestamp_utc`: Unix timestamp (seconds since epoch) when data was collected
- `readings`: Key-value map of sensor readings (e.g., "temperature_c" -> 22.5)
- `sequence_num`: Monotonically increasing sequence number for detecting missed packets

## Nanopb Implementation Details

- Maps are implemented using Nanopb's callback mechanism
- Encoding/decoding functions are provided in `AkitaSmartCityServices.cpp`
- The `readings` map uses `std::map<std::string, float>` in C++

## MQTT JSON Format (Gateway Output)

Gateways convert `SensorData` to JSON for MQTT publishing:

```json
{
  "node_id": "a1b2c3d4",
  "sensor_id": "BME280-Floor1",
  "timestamp_utc": 1714148000,
  "sequence_num": 123,
  "readings": {
    "temperature_c": 22.5,
    "humidity_pct": 45.8,
    "pressure_pa": 101325.0
  }
}
```

## Topic Structure

MQTT topics follow: `<base_topic>/sensor/<service_id>/<originating_node_hex>/<sensor_id>`

Example: `akita/smartcity/sensor/99/a1b2c3d4/BME280-Floor1`

    # Set values (use correct preference level, e.g., EDITABLE)
    prefs.set('role', 3, meshtastic.prefs.WarningLevel.EDITABLE)
    prefs.set('wifi_ssid', 'MyCityWiFi', meshtastic.prefs.WarningLevel.EDITABLE)
    # ... set other preferences ...

    # Write changes back to device
    interface.localNode.setPrefs(prefs)

    # Optional: Reboot device
    # interface.localNode.reboot()
    interface.close()
    ```
4.  Ensure changes are saved (the API might handle commit implicitly, or you might need `interface.localNode.writePrefs()`).

## Default Values

If a key is not found in the `Preferences` storage, the default value defined in `ASCSConfig.h` will be used by the plugin during initialization. It's recommended to explicitly set all required parameters for production deployments.
