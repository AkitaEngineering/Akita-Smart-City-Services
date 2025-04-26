# Akita Smart City Services (ASCS) - System Architecture

This document provides a high-level overview of the ASCS architecture.

## Core Components

* **Sensor Nodes:** Deployed devices equipped with physical sensors (e.g., environmental, utility meters, parking sensors). They run Meshtastic firmware with the ASCS plugin configured in `SENSOR` role. Their primary function is to read sensor data periodically or based on events, format it using the `SensorData` Protocol Buffer message, and transmit it over the Meshtastic LoRa mesh network.
* **Aggregator Nodes (Optional):** Intermediate nodes running Meshtastic with the ASCS plugin in `AGGREGATOR` role. They listen for `SensorData` packets from nearby sensor nodes and forward them towards known Gateway nodes. This can help extend range and potentially reduce redundant transmissions in dense areas. The current implementation performs simple forwarding.
* **Gateway Nodes:** Critical nodes running Meshtastic with the ASCS plugin in `GATEWAY` role. These nodes have both a LoRa radio (for the mesh network) and an IP network connection (WiFi). They receive `SensorData` packets from the mesh, decode them, format the data (typically as JSON), and publish it to a configured MQTT broker over the IP network. They also handle buffering if the IP network or MQTT broker is temporarily unavailable.
* **Meshtastic Network:** The underlying LoRa mesh network managed by the Meshtastic firmware. ASCS leverages Meshtastic for radio communication, routing, node discovery, and time synchronization. ASCS uses a dedicated PortNum for its application-specific packets.
* **MQTT Broker:** An external message broker (e.g., Mosquitto, HiveMQ) accessible via IP. Gateways publish sensor data to specific topics on the broker.
* **Backend Infrastructure:** Cloud or on-premise servers/applications that subscribe to the MQTT topics, process the incoming sensor data, store it in databases, and potentially provide dashboards, alerts, or control interfaces for city services.

## Data Flow

1.  **Sensor Reading:** A Sensor Node reads data from its attached physical sensor(s).
2.  **Data Formatting:** The Sensor Node uses the ASCS plugin to format the readings into a `SensorData` Protocol Buffer message, including sensor ID, timestamp, and a map of readings.
3.  **Transmission (Sensor -> Mesh):** The Sensor Node determines the destination (broadcast, discovered gateway, or configured target) and uses the ASCS plugin (`sendMessage`) to transmit the `SmartCityPacket` (containing `SensorData`) over the Meshtastic LoRa mesh.
4.  **Relaying (Optional - Aggregator):** An Aggregator Node may receive the packet. If it knows of a suitable Gateway, it re-transmits the *same* `SmartCityPacket` towards that Gateway.
5.  **Reception (Gateway):** A Gateway Node receives the `SmartCityPacket` on the designated ASCS PortNum.
6.  **Decoding & Processing (Gateway):** The Gateway's ASCS plugin decodes the `SmartCityPacket` and extracts the `SensorData`.
7.  **Buffering (Gateway):** If the MQTT connection is unavailable, the Gateway encodes the received packet and appends it to a local buffer file (SPIFFS/LittleFS).
8.  **MQTT Publishing (Gateway):** If MQTT is connected, the Gateway formats the `SensorData` (including the readings map) into a JSON payload. It constructs a topic string based on configuration and packet details (originating node ID, sensor ID, etc.) and publishes the JSON payload to the MQTT broker.
9.  **Buffer Processing (Gateway):** When MQTT reconnects, the Gateway periodically reads packets from its buffer file, decodes them, formats them as JSON, publishes them to MQTT, and removes them from the buffer.
10. **Backend Consumption:** Backend applications subscribe to the relevant MQTT topics, receive the JSON data, and process it for storage, analysis, visualization, etc.

## Diagram (Conceptual)

+-------------+   LoRa Mesh   +--------------+   LoRa Mesh   +-------------+      IP Network      +--------------+      IP Network      +--------------------+| Sensor Node | <-----------> | Aggregator   | <-----------> | Gateway     | <------------------> | MQTT Broker  | <------------------> | Backend Services   || (ASCS Role 1)|   (Packet)    | Node (Opt.)  |   (Packet)    | Node        |      (MQTT JSON)     | (e.g.,       |      (DB, API)       | (Database, Dashbd) || - Read Data |               | (ASCS Role 2)|               | (ASCS Role 3)|                      | Mosquitto)   |                      | - Store Data       || - Format PB |               | - Forward Pkt|               | - Decode PB |                      +--------------+                      | - Analyze          || - Send Mesh |               +--------------+               | - Format JSON|                                                            | - Visualize        |+-------------+                                              | - Publish MQTT                                                           +--------------------+| - Buffer Data|+-------------+
*(This diagram shows the primary data flow. Service Discovery packets are typically broadcast periodically by all nodes.)*

