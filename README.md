# Akita Smart City Services (ASCS)

Akita Smart City Services (ASCS) is a Meshtastic plugin designed to facilitate IoT-based smart city infrastructure by enabling communication between sensors, aggregators, and gateway nodes over a wireless mesh network.

## Overview

This project implements a smart city service discovery and data aggregation system. Nodes in the network can assume one of three roles:
- **Sensor**: Captures environmental data (e.g., temperature, humidity) and sends it over the mesh network.
- **Aggregator**: Receives sensor data and forwards it to a gateway or another aggregator.
- **Gateway**: Connects the mesh network to external services via MQTT, publishing sensor data to an MQTT broker.

## Features

- Service discovery: Nodes identify available services and establish communication.
- MQTT integration: Gateways publish sensor data to an external MQTT broker.
- Wi-Fi connectivity: Gateways connect to the internet for data transmission.
- Message encoding/decoding using Protocol Buffers.
- JSON payloads for MQTT communication.

## Prerequisites

- Meshtastic-compatible hardware
- Wi-Fi network
- MQTT broker
- Arduino IDE or PlatformIO

## Configuration

Update the following values in `AkitaSmartCityServices.cpp` to match your environment:

```cpp
#define MQTT_SERVER "your_mqtt_server.com"
#define MQTT_PORT 1883
#define MQTT_USER "your_mqtt_user"
#define MQTT_PASSWORD "your_mqtt_password"
#define WIFI_SSID "your_wifi_ssid"
#define WIFI_PASSWORD "your_wifi_password"
```

Set the node role and service ID in the `loadConfig` function:

```cpp
myServiceId = 1;  // Unique service ID
myNodeRole = 0;   // 0: Sensor, 1: Aggregator, 2: Gateway
```

## Installation

1. Clone the repository.
2. Open the project in the Arduino IDE or PlatformIO.
3. Install the required libraries:
    - Meshtastic
    - PubSubClient
    - ArduinoJson
    - Protocol Buffers for Arduino
4. Upload the code to your Meshtastic device.

## Usage

- **Sensors** periodically send environmental data.
- **Aggregators** forward data to gateways or other aggregators.
- **Gateways** publish the data to an MQTT broker.

To start the service, call:

```cpp
AkitaSmartCityServices.begin();
```

Ensure the `update()` function is called regularly within the main loop:

```cpp
void loop() {
    AkitaSmartCityServices.update();
}
```

## Troubleshooting

- Ensure Wi-Fi credentials are correct for gateway nodes.
- Check MQTT broker settings and topic subscriptions.
- Confirm hardware compatibility with Meshtastic firmware.
- Verify node roles and service IDs are unique and correctly set.

## License

This project is licensed under the GNU General Public License v3.0.

## Contributing

Pull requests are welcome. For major changes, please open an issue first to discuss your proposed changes.

## Akita Engineering
This project is developed and maintained by Akita Engineering. We are dedicated to creating innovative solutions for LoRa and Meshtastic networks.

