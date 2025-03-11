# Akita Smart City Services (ASCS)

This Meshtastic plugin, Akita Smart City Services, enables the creation of a distributed smart city IoT network using Meshtastic devices. It supports service discovery, sensor data transmission, and MQTT integration for gateway nodes.

## Features

* **Service Discovery:** Allows nodes to announce and discover available services within the Meshtastic network.
* **Sensor Data Transmission:** Facilitates the transmission of sensor data between nodes.
* **Node Roles:** Supports different node roles:
    * **Sensor:** Collects and transmits sensor data.
    * **Aggregator:** Receives and forwards sensor data to other nodes or gateways.
    * **Gateway:** Bridges the Meshtastic network with an MQTT broker, enabling data transmission to cloud or server infrastructure.
* **MQTT Integration:** Gateway nodes publish sensor data to an MQTT broker.
* **Configuration Management:** Uses Arduino `Preferences` library for storing and retrieving configuration parameters.
* **Protocol Buffers:** Employs Protocol Buffers for efficient and structured message serialization and deserialization.
* **Service Table Management:** Manages a table of discovered services with timeouts.
* **WiFi Management:** Gateway nodes manage WiFi connections.
* **MQTT Reconnection:** Gateway nodes handle MQTT reconnection with a time-based delay.

## Requirements

* Arduino IDE
* Meshtastic library
* PubSubClient library
* ArduinoJson library
* Protocol Buffers library
* WiFi library (for gateway nodes)
* An MQTT broker (for gateway nodes)

## Installation

1.  **Install Libraries:** Install the required libraries through the Arduino Library Manager or by manually downloading and installing them.
2.  **Create Files:** Create `AkitaSmartCityServices.cpp` and `AkitaSmartCityServices.h` files in your Arduino sketch directory. Copy the provided code into these files.
3.  **Create Protobuf files:** Create the SmartCity.proto file, and generate the .pb.c and .pb.h files, and include them in your project.
4.  **Configure:**
    * Open your Arduino sketch and include the `AkitaSmartCityServices.h` header file.
    * Create an instance of the `AkitaSmartCityServices` class.
    * Call the `begin()` method of the class in your `setup()` function.
    * Call the `update()` method of the class in your `loop()` function.
    * For gateway nodes, configure the WiFi and MQTT settings using the Arduino Preferences library.
5.  **Upload:** Upload the sketch to your Meshtastic device.

## Configuration

Configuration parameters are stored using the Arduino `Preferences` library.

* **Service ID:** A unique identifier for the service provided by the node.
* **Node Role:** Defines the role of the node (sensor, aggregator, gateway).
* **MQTT Server:** The address of the MQTT broker (gateway nodes).
* **MQTT Port:** The port number of the MQTT broker (gateway nodes).
* **MQTT User:** The username for MQTT authentication (gateway nodes).
* **MQTT Password:** The password for MQTT authentication (gateway nodes).
* **WiFi SSID:** The SSID of the WiFi network (gateway nodes).
* **WiFi Password:** The password of the WiFi network (gateway nodes).

## Usage

* Nodes will automatically discover each other's services.
* Sensor nodes will transmit sensor data.
* Aggregator nodes will forward sensor data.
* Gateway nodes will publish sensor data to the configured MQTT broker.
* The service table will be kept up to date, and old services will time out.

## Protocol Buffers

The plugin uses Protocol Buffers for message serialization. The `SmartCity.proto` file defines the message structure.

## MQTT Topics

Gateway nodes publish sensor data to MQTT topics in the following format:

smartcity/<service_id>/<node_id>


## Contributing

Contributions are welcome! Please submit pull requests or bug reports.
