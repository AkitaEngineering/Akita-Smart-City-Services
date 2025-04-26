#include "AkitaSmartCityServices.h"
#include "meshtastic.h"
#include "plugin_api.h"
#include "pb_encode.h"
#include "pb_decode.h"
#include "mesh_packet.h" // For meshPacket definition
#include "globals.h"     // Access to global objects like radio

// Required Libraries (conditional includes for Gateway role)
#include <Preferences.h>
#ifdef ASCS_ROLE_GATEWAY // Only include WiFi/MQTT if Gateway role might be used
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h> // For formatting MQTT payload
#endif

// Nanopb includes
#include "pb_common.h"
#include "pb.h"

// Static instance pointer initialization for MQTT callback
AkitaSmartCityServices* AkitaSmartCityServices::s_instance = nullptr;

// --- Constructor / Destructor ---

AkitaSmartCityServices::AkitaSmartCityServices(const char *name) : MeshtasticPlugin(name) {
    // Initialize pointer members to nullptr
    m_wifiClient = nullptr;
    m_mqttClient = nullptr;
    m_preferences = nullptr;
    s_instance = this; // Set static instance pointer
}

AkitaSmartCityServices::~AkitaSmartCityServices() {
    // Clean up allocated memory
    delete m_mqttClient;
    delete m_wifiClient;
    delete m_preferences;
    if (s_instance == this) {
        s_instance = nullptr; // Clear static instance if this was the one
    }
}

// --- Meshtastic Plugin API Methods ---

void AkitaSmartCityServices::init(const MeshtasticAPI *api) {
    m_api = api;
    Log.printf(LOG_LEVEL_INFO, "%s: Initializing...\n", getName());

    // Initialize Preferences
    m_preferences = new Preferences();
    m_preferences->begin("ascs", false); // Namespace "ascs", read/write mode

    // Load configuration from NVS
    loadConfig();

    Log.printf(LOG_LEVEL_INFO, "%s: Role=%d, ServiceID=%lu, TargetNode=0x%lx, ReadInterval=%lums, DiscoveryInterval=%lums\n",
               getName(), m_nodeRole, m_serviceId, m_targetNodeId, m_sensorReadIntervalMs, m_discoveryIntervalMs);

    // Initialize network clients if this node is a Gateway
    if (m_nodeRole == ServiceDiscovery_Role_GATEWAY) {
        #ifdef ASCS_ROLE_GATEWAY
            Log.printf(LOG_LEVEL_INFO, "%s: Initializing Gateway network components...\n", getName());
            m_wifiClient = new WiFiClient();
            m_mqttClient = new PubSubClient(*m_wifiClient);
            m_mqttClient->setServer(m_mqttServer.c_str(), m_mqttPort);
            m_mqttClient->setCallback(mqttCallback); // Set static callback
            connectWiFi(); // Initial connection attempt
        #else
            Log.printf(LOG_LEVEL_WARNING, "%s: Gateway role configured but WiFi/MQTT support not compiled in!\n", getName());
            m_nodeRole = ServiceDiscovery_Role_UNKNOWN; // Force role to unknown if libs missing
        #endif
    }

    // Send initial service discovery announcement
    sendServiceDiscovery();
    m_lastDiscoverySendTime = millis();
    m_lastServiceCleanupTime = millis();

    Log.printf(LOG_LEVEL_INFO, "%s: Initialization complete.\n", getName());
}

void AkitaSmartCityServices::loop() {
    unsigned long now = millis();

    // Role-specific periodic actions
    if (m_nodeRole == ServiceDiscovery_Role_SENSOR && m_sensor != nullptr) {
        if (now - m_lastSensorReadTime >= m_sensorReadIntervalMs) {
            runSensorLogic();
            m_lastSensorReadTime = now;
        }
    } else if (m_nodeRole == ServiceDiscovery_Role_GATEWAY) {
        #ifdef ASCS_ROLE_GATEWAY
            checkWiFiConnection(); // Ensure WiFi is up
            checkMQTTConnection(); // Ensure MQTT is connected and process incoming messages
            if (m_mqttClient && m_mqttClient->connected()) {
                 m_mqttClient->loop(); // Allow MQTT client to process messages
            }
        #endif
    }
    // Aggregator role primarily reacts to incoming messages, no major periodic task here yet.

    // Periodic Service Discovery broadcast
    if (now - m_lastDiscoverySendTime >= m_discoveryIntervalMs) {
        sendServiceDiscovery();
        m_lastDiscoverySendTime = now;
    }

    // Periodic Service Table cleanup
    if (now - m_lastServiceCleanupTime >= m_serviceTimeoutMs / 2) { // Cleanup more often than timeout
        cleanupServiceTable();
        m_lastServiceCleanupTime = now;
    }
}

bool AkitaSmartCityServices::handleReceived(const meshPacket &packet) {
    // Check if the packet is for our application port number
    if (packet.decoded.portnum == ASCS_PORT_NUM) {
        Log.printf(LOG_LEVEL_DEBUG, "%s: Received packet on port %d from 0x%lx, size %d\n",
                   getName(), ASCS_PORT_NUM, packet.from, packet.decoded.payloadlen);

        SmartCityPacket scp = SmartCityPacket_init_zero;
        pb_istream_t stream = pb_istream_from_buffer(packet.decoded.payload, packet.decoded.payloadlen);

        if (pb_decode(&stream, SmartCityPacket_fields, &scp)) {
            Log.printf(LOG_LEVEL_DEBUG, "%s: Successfully decoded SmartCityPacket\n", getName());

            switch (scp.which_payload) {
                case SmartCityPacket_discovery_tag:
                    Log.printf(LOG_LEVEL_DEBUG, "%s: Handling ServiceDiscovery from 0x%lx\n", getName(), packet.from);
                    handleServiceDiscovery(scp.payload.discovery, packet.from);
                    break;
                case SmartCityPacket_sensor_data_tag:
                    Log.printf(LOG_LEVEL_DEBUG, "%s: Handling SensorData from 0x%lx\n", getName(), packet.from);
                    handleSensorData(scp.payload.sensor_data, packet.from);
                    break;
                // case SmartCityPacket_config_tag:
                //     Log.printf(LOG_LEVEL_DEBUG, "%s: Handling ServiceConfig from 0x%lx\n", getName(), packet.from);
                //     // handleServiceConfig(scp.payload.config, packet.from); // Implement if needed
                //     break;
                default:
                    Log.printf(LOG_LEVEL_WARNING, "%s: Received unknown payload type (%d) in SmartCityPacket\n", getName(), scp.which_payload);
                    break;
            }
            return true; // Packet was handled by this plugin

        } else {
            Log.printf(LOG_LEVEL_ERROR, "%s: Failed to decode SmartCityPacket from 0x%lx: %s\n",
                       getName(), packet.from, PB_GET_ERROR(&stream));
            return false; // Decoding failed
        }
    }
    return false; // Not our port number
}

// --- Public Configuration Methods ---

void AkitaSmartCityServices::setSensor(std::unique_ptr<SensorInterface> sensor) {
    m_sensor = std::move(sensor);
    Log.printf(LOG_LEVEL_INFO, "%s: Sensor implementation set.\n", getName());
}

ServiceDiscovery_Role AkitaSmartCityServices::getNodeRole() const {
    return m_nodeRole;
}


// --- Internal Helper Methods ---

void AkitaSmartCityServices::loadConfig() {
    Log.printf(LOG_LEVEL_DEBUG, "%s: Loading configuration from Preferences...\n", getName());
    m_nodeRole = (ServiceDiscovery_Role)m_preferences->getUInt("role", ASCS_DEFAULT_ROLE);
    m_serviceId = m_preferences->getUInt("service_id", ASCS_DEFAULT_SERVICE_ID);
    m_targetNodeId = m_preferences->getUInt("target_node", ASCS_DEFAULT_TARGET_NODE);
    m_sensorReadIntervalMs = m_preferences->getUInt("read_int", ASCS_DEFAULT_SENSOR_READ_INTERVAL_MS);
    m_discoveryIntervalMs = m_preferences->getUInt("disc_int", ASCS_DEFAULT_DISCOVERY_INTERVAL_MS);
    m_serviceTimeoutMs = m_preferences->getUInt("svc_tout", ASCS_DEFAULT_SERVICE_TIMEOUT_MS);

    if (m_nodeRole == ServiceDiscovery_Role_GATEWAY) {
        #ifdef ASCS_ROLE_GATEWAY
            m_wifiSsid = m_preferences->getString("wifi_ssid", ASCS_DEFAULT_WIFI_SSID).c_str();
            m_wifiPassword = m_preferences->getString("wifi_pass", ASCS_DEFAULT_WIFI_PASSWORD).c_str();
            m_mqttServer = m_preferences->getString("mqtt_srv", ASCS_DEFAULT_MQTT_SERVER).c_str();
            m_mqttPort = m_preferences->getInt("mqtt_port", ASCS_DEFAULT_MQTT_PORT);
            m_mqttUser = m_preferences->getString("mqtt_user", ASCS_DEFAULT_MQTT_USER).c_str();
            m_mqttPassword = m_preferences->getString("mqtt_pass", ASCS_DEFAULT_MQTT_PASSWORD).c_str();
            m_mqttBaseTopic = m_preferences->getString("mqtt_topic", ASCS_DEFAULT_MQTT_BASE_TOPIC).c_str();

            Log.printf(LOG_LEVEL_DEBUG, "%s: Gateway Config: WiFi=%s, MQTT=%s:%d, User=%s, BaseTopic=%s\n",
                       getName(), m_wifiSsid.c_str(), m_mqttServer.c_str(), m_mqttPort, m_mqttUser.c_str(), m_mqttBaseTopic.c_str());
        #endif
    }
     Log.printf(LOG_LEVEL_DEBUG, "%s: Configuration loaded.\n", getName());
}

void AkitaSmartCityServices::saveConfig() {
    // Example: Saving the role if it were changed dynamically
    // m_preferences->putUInt("role", m_nodeRole);
    // Log.printf(LOG_LEVEL_INFO, "%s: Configuration saved.\n", getName());
    // Note: Requires Preferences to be opened in read/write mode.
}

#ifdef ASCS_ROLE_GATEWAY
void AkitaSmartCityServices::connectWiFi() {
    if (WiFi.status() == WL_CONNECTED) {
        return; // Already connected
    }

    Log.printf(LOG_LEVEL_INFO, "%s: Connecting to WiFi SSID: %s\n", getName(), m_wifiSsid.c_str());
    WiFi.mode(WIFI_STA); // Set station mode
    WiFi.begin(m_wifiSsid.c_str(), m_wifiPassword.c_str());

    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 15000) { // 15 second timeout
        delay(500);
        Serial.print("."); // Use Serial for direct feedback during connection
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Log.printf(LOG_LEVEL_INFO, "%s: WiFi connected. IP: %s\n", getName(), WiFi.localIP().toString().c_str());
        // Attempt MQTT connection now that WiFi is up
        connectMQTT();
    } else {
        Log.printf(LOG_LEVEL_ERROR, "%s: WiFi connection failed!\n", getName());
        WiFi.disconnect(true); // Ensure WiFi is off if connection failed
        WiFi.mode(WIFI_OFF);
    }
}

void AkitaSmartCityServices::checkWiFiConnection() {
     if (m_nodeRole != ServiceDiscovery_Role_GATEWAY) return;

     if (WiFi.status() != WL_CONNECTED) {
         Log.printf(LOG_LEVEL_WARNING, "%s: WiFi disconnected. Attempting reconnect...\n", getName());
         connectWiFi(); // Attempt to reconnect
     }
}


void AkitaSmartCityServices::connectMQTT() {
    if (!m_mqttClient || m_mqttClient->connected()) {
        return; // No client or already connected
    }

    if (WiFi.status() != WL_CONNECTED) {
        Log.printf(LOG_LEVEL_WARNING, "%s: Cannot connect MQTT, WiFi is down.\n", getName());
        return; // Need WiFi first
    }

    Log.printf(LOG_LEVEL_INFO, "%s: Attempting MQTT connection to %s:%d...\n", getName(), m_mqttServer.c_str(), m_mqttPort);
    // Create a unique client ID
    String clientId = "meshtastic-ascs-";
    clientId += String(m_api->getMyNodeInfo()->node_num, HEX); // Use Meshtastic node ID

    bool result;
    if (!m_mqttUser.empty()) {
        result = m_mqttClient->connect(clientId.c_str(), m_mqttUser.c_str(), m_mqttPassword.c_str());
    } else {
        result = m_mqttClient->connect(clientId.c_str());
    }

    if (result) {
        Log.printf(LOG_LEVEL_INFO, "%s: MQTT connected.\n", getName());
        // Subscribe to any relevant topics if needed (e.g., for remote commands)
        // mqttClient->subscribe("akita/smartcity/gateway/command");
    } else {
        Log.printf(LOG_LEVEL_ERROR, "%s: MQTT connection failed, rc=%d. Will retry later.\n", getName(), m_mqttClient->state());
    }
    m_lastMqttReconnectAttempt = millis(); // Record attempt time regardless of success
}

void AkitaSmartCityServices::checkMQTTConnection() {
    if (m_nodeRole != ServiceDiscovery_Role_GATEWAY || !m_mqttClient) return;

    if (!m_mqttClient->connected()) {
        unsigned long now = millis();
        // Only attempt reconnect periodically
        if (now - m_lastMqttReconnectAttempt > ASCS_DEFAULT_MQTT_RECONNECT_INTERVAL_MS) {
             Log.printf(LOG_LEVEL_WARNING, "%s: MQTT disconnected. Attempting reconnect...\n", getName());
             connectMQTT(); // Attempt to reconnect
        }
    }
    // Note: m_mqttClient->loop() is called in the main loop() to handle keepalives and incoming messages
}


// Static MQTT callback function
void AkitaSmartCityServices::mqttCallback(char *topic, byte *payload, unsigned int length) {
    if (s_instance) {
        Log.printf(LOG_LEVEL_INFO, "%s: MQTT message received on topic: %s\n", s_instance->getName(), topic);
        // Process incoming MQTT messages here if needed (e.g., commands)
        // Example: Check topic, parse payload (JSON, etc.), perform action
        payload[length] = '\0'; // Null-terminate payload
        std::string message((char*)payload);
        Log.printf(LOG_LEVEL_DEBUG, "%s: MQTT Payload: %s\n", s_instance->getName(), message.c_str());
    } else {
         Log.printf(LOG_LEVEL_ERROR, "MQTT Callback: Static instance pointer is null!\n");
    }
}
#else
// Define stubs for WiFi/MQTT functions if Gateway role is not compiled in
void AkitaSmartCityServices::connectWiFi() { }
void AkitaSmartCityServices::checkWiFiConnection() { }
void AkitaSmartCityServices::connectMQTT() { }
void AkitaSmartCityServices::checkMQTTConnection() { }
void AkitaSmartCityServices::mqttCallback(char*, byte*, unsigned int) { }
#endif // ASCS_ROLE_GATEWAY


void AkitaSmartCityServices::handleServiceDiscovery(const ServiceDiscovery &discovery, uint32_t fromNode) {
    // Update our table of known services
    updateServiceTable(fromNode, discovery.node_role, discovery.service_id);

    // Optional: If we are a sensor/aggregator and received a discovery from a gateway,
    // we could potentially update our targetNodeId if it's currently 0.
    // if ((m_nodeRole == ServiceDiscovery_Role_SENSOR || m_nodeRole == ServiceDiscovery_Role_AGGREGATOR) &&
    //     discovery.node_role == ServiceDiscovery_Role_GATEWAY &&
    //     m_targetNodeId == 0) {
    //     Log.printf(LOG_LEVEL_INFO, "%s: Discovered Gateway 0x%lx, setting as potential target.\n", getName(), fromNode);
    //     // Decide if we automatically adopt it or just note it. For now, just log.
    // }
}

void AkitaSmartCityServices::handleSensorData(const SensorData &sensorData, uint32_t fromNode) {
    // Regardless of role, update the service table entry for the sender (they are implicitly a sensor or aggregator)
    // We don't know their *configured* role just from sensor data, so maybe don't update role here.
    // updateServiceTable(fromNode, ServiceDiscovery_Role_UNKNOWN, 0); // Just update lastSeen

    switch (m_nodeRole) {
        case ServiceDiscovery_Role_AGGREGATOR:
            runAggregatorLogic(sensorData, fromNode);
            break;
        case ServiceDiscovery_Role_GATEWAY:
            runGatewayLogic(sensorData, fromNode);
            break;
        case ServiceDiscovery_Role_SENSOR:
            // Sensors typically shouldn't receive sensor data from others, but log if they do.
            Log.printf(LOG_LEVEL_DEBUG, "%s: Sensor node received unexpected SensorData from 0x%lx\n", getName(), fromNode);
            break;
        default: // UNKNOWN role
             Log.printf(LOG_LEVEL_DEBUG, "%s: Node with UNKNOWN role received SensorData from 0x%lx\n", getName(), fromNode);
            break;
    }
}

bool AkitaSmartCityServices::sendMessage(uint32_t toNode, const SmartCityPacket &packet) {
    uint8_t buffer[SmartCityPacket_size]; // Use max size defined by nanopb
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    if (pb_encode(&stream, SmartCityPacket_fields, &packet)) {
        Log.printf(LOG_LEVEL_DEBUG, "%s: Sending packet (type %d) to 0x%lx, size %d bytes\n",
                   getName(), packet.which_payload, toNode, stream.bytes_written);

        // Use Meshtastic API to send the data
        return m_api->sendData(toNode, buffer, stream.bytes_written, ASCS_PORT_NUM, Data_WANT_ACK_DEFAULT); // Use default ACK setting

    } else {
        Log.printf(LOG_LEVEL_ERROR, "%s: Failed to encode SmartCityPacket: %s\n", getName(), PB_GET_ERROR(&stream));
        return false;
    }
}

void AkitaSmartCityServices::sendServiceDiscovery(uint32_t toNode /*= ASCS_BROADCAST_ADDR*/) {
    Log.printf(LOG_LEVEL_DEBUG, "%s: Sending Service Discovery to 0x%lx\n", getName(), toNode);
    SmartCityPacket packet = SmartCityPacket_init_zero;
    packet.which_payload = SmartCityPacket_discovery_tag;
    packet.payload.discovery.node_role = m_nodeRole;
    packet.payload.discovery.service_id = m_serviceId;

    sendMessage(toNode, packet);
}

void AkitaSmartCityServices::sendSensorData(const SensorData &sensorData) {
    uint32_t target = m_targetNodeId;

    // If no specific target is configured, try to find a gateway from the service table
    if (target == 0 || target == ASCS_BROADCAST_ADDR) {
        target = findGatewayNode(); // Find best known gateway
         Log.printf(LOG_LEVEL_DEBUG, "%s: No target configured, found Gateway 0x%lx\n", getName(), target);
    }

    // If still no target found, broadcast
    if (target == 0) {
        target = ASCS_BROADCAST_ADDR;
        Log.printf(LOG_LEVEL_DEBUG, "%s: No gateway found, broadcasting sensor data.\n", getName());
    }

    SmartCityPacket packet = SmartCityPacket_init_zero;
    packet.which_payload = SmartCityPacket_sensor_data_tag;
    packet.payload.sensor_data = sensorData; // Copy sensor data into packet

    sendMessage(target, packet);
}


// --- Role-Specific Logic ---

void AkitaSmartCityServices::runSensorLogic() {
    if (!m_sensor) {
        Log.printf(LOG_LEVEL_WARNING, "%s: Sensor role active, but no sensor implementation provided!\n", getName());
        return;
    }

    Log.printf(LOG_LEVEL_DEBUG, "%s: Reading sensor data...\n", getName());
    SensorData data = SensorData_init_zero;
    std::map<std::string, float> readingsMap;

    if (m_sensor->readData(readingsMap)) {
        Log.printf(LOG_LEVEL_DEBUG, "%s: Sensor read successful (%d readings).\n", getName(), readingsMap.size());

        // Populate SensorData message
        strncpy(data.sensor_id, m_sensor->getSensorId().c_str(), sizeof(data.sensor_id) - 1);
        data.sensor_id[sizeof(data.sensor_id) - 1] = '\0'; // Ensure null termination

        data.timestamp_utc = m_api->getAdjustedTime(); // Use Meshtastic's time
        data.sequence_num = ++m_sensorSequenceNum;

        // Encode the map into the protobuf map field (requires nanopb callbacks)
        // This is the complex part with nanopb maps. We need encode callbacks.

        // ** WORKAROUND for nanopb map complexity in this example: **
        // ** Serialize map to JSON string and put in a dedicated string field, OR **
        // ** Define fixed fields in SensorData for common readings (temp, humidity, etc.) **
        // Let's choose fixed fields for simplicity in this initial version.
        // --> This requires changing SmartCity.proto! <--
        // Assuming proto was changed to have:
        // optional float temperature_c = 10;
        // optional float humidity_pct = 11;
        // optional float battery_v = 12;

        // data.has_temperature_c = readingsMap.count("temperature_c");
        // if(data.has_temperature_c) data.temperature_c = readingsMap["temperature_c"];
        // data.has_humidity_pct = readingsMap.count("humidity_pct");
        // if(data.has_humidity_pct) data.humidity_pct = readingsMap["humidity_pct"];
        // data.has_battery_v = readingsMap.count("battery_v");
        // if(data.has_battery_v) data.battery_v = readingsMap["battery_v"];

        // ** IF sticking with the map<string, float> readings = 3; **
        // We need to implement encode/decode callbacks for the map field.
        // This involves functions passed to nanopb during encode/decode.
        // See nanopb documentation for 'pb_callback_t'.
        // For now, we'll skip populating the map to keep the example simpler,
        // acknowledging this is incomplete for the map approach.
         Log.printf(LOG_LEVEL_WARNING, "%s: Nanopb map encoding not implemented in this example. Sensor data map will be empty.\n", getName());
        // TODO: Implement nanopb map encoding callbacks for SensorData.readings


        // Send the data
        sendSensorData(data);

    } else {
        Log.printf(LOG_LEVEL_ERROR, "%s: Failed to read sensor data.\n", getName());
    }
}

void AkitaSmartCityServices::runAggregatorLogic(const SensorData &sensorData, uint32_t fromNode) {
     Log.printf(LOG_LEVEL_INFO, "%s: Aggregator received sensor data from 0x%lx.\n", getName(), fromNode);

    // Simple Aggregator: Forward data towards a known or configured gateway.
    uint32_t targetGateway = m_targetNodeId; // Use configured target first

    if (targetGateway == 0 || targetGateway == ASCS_BROADCAST_ADDR) {
        targetGateway = findGatewayNode(); // Try to find one via discovery
         Log.printf(LOG_LEVEL_DEBUG, "%s: Aggregator trying to find gateway, result: 0x%lx\n", getName(), targetGateway);
    }

    if (targetGateway != 0 && targetGateway != ASCS_BROADCAST_ADDR) {
        Log.printf(LOG_LEVEL_INFO, "%s: Aggregator forwarding data from 0x%lx to Gateway 0x%lx\n", getName(), fromNode, targetGateway);
        // Forward the *exact same* sensor data message
        SmartCityPacket packet = SmartCityPacket_init_zero;
        packet.which_payload = SmartCityPacket_sensor_data_tag;
        packet.payload.sensor_data = sensorData; // Copy data
        sendMessage(targetGateway, packet);
    } else {
        Log.printf(LOG_LEVEL_WARNING, "%s: Aggregator received data from 0x%lx, but no target gateway known. Dropping.\n", getName(), fromNode);
        // Optionally, could broadcast if no gateway found, but that might flood the network.
    }
}

void AkitaSmartCityServices::runGatewayLogic(const SensorData &sensorData, uint32_t fromNode) {
    Log.printf(LOG_LEVEL_INFO, "%s: Gateway received sensor data from 0x%lx.\n", getName(), fromNode);

    #ifdef ASCS_ROLE_GATEWAY
        // Publish the received data to MQTT
        publishMqtt(sensorData, fromNode);
    #else
         Log.printf(LOG_LEVEL_WARNING, "%s: Gateway logic called, but WiFi/MQTT support not compiled in!\n", getName());
    #endif
}

// --- Service Discovery Management ---

void AkitaSmartCityServices::updateServiceTable(uint32_t nodeId, ServiceDiscovery_Role role, uint32_t serviceId) {
    if (nodeId == m_api->getMyNodeInfo()->node_num) {
        return; // Don't store ourselves
    }
    unsigned long now = millis();
    m_serviceTable[nodeId] = {role, serviceId, now};
    Log.printf(LOG_LEVEL_DEBUG, "%s: Updated service table for node 0x%lx: Role=%d, ServiceID=%lu, LastSeen=%lu\n",
               getName(), nodeId, role, serviceId, now);
}

void AkitaSmartCityServices::cleanupServiceTable() {
    unsigned long now = millis();
    int removedCount = 0;
    for (auto it = m_serviceTable.begin(); it != m_serviceTable.end(); /* no increment here */) {
        if (now - it->second.lastSeen > m_serviceTimeoutMs) {
            Log.printf(LOG_LEVEL_INFO, "%s: Service timed out for node 0x%lx\n", getName(), it->first);
            it = m_serviceTable.erase(it); // Erase returns iterator to the next element
            removedCount++;
        } else {
            ++it; // Only increment if not erased
        }
    }
     if (removedCount > 0) {
        Log.printf(LOG_LEVEL_DEBUG, "%s: Removed %d timed-out service(s).\n", getName(), removedCount);
     }
}

uint32_t AkitaSmartCityServices::findGatewayNode() {
    uint32_t bestGateway = 0;
    unsigned long latestSeen = 0;

    for (const auto& pair : m_serviceTable) {
        if (pair.second.role == ServiceDiscovery_Role_GATEWAY) {
            // Simple strategy: pick the most recently seen gateway
            if (pair.second.lastSeen > latestSeen) {
                latestSeen = pair.second.lastSeen;
                bestGateway = pair.first; // pair.first is the Node ID (key)
            }
            // Could add more complex logic: prefer specific service IDs, check RSSI/SNR via NodeDB, etc.
        }
    }
    return bestGateway;
}


// --- MQTT Publishing (Gateway Role) ---
#ifdef ASCS_ROLE_GATEWAY
void AkitaSmartCityServices::publishMqtt(const SensorData &sensorData, uint32_t fromNode) {
    if (!m_mqttClient || !m_mqttClient->connected()) {
        Log.printf(LOG_LEVEL_WARNING, "%s: Cannot publish MQTT, client not connected.\n", getName());
        return;
    }

    // Construct Topic: base/role/service_id/from_node_id/sensor_id
    // Example: akita/smartcity/sensor/1/a1b2c3d4/BME280-Floor1
    char fromNodeHex[9];
    snprintf(fromNodeHex, sizeof(fromNodeHex), "%08lx", fromNode); // Format node ID as hex string

    std::string topic = m_mqttBaseTopic;
    topic += "/sensor/"; // Assuming data always originates from a sensor role conceptually
    topic += std::to_string(m_serviceId); // Could use service ID from discovery if available? Using gateway's for now.
    topic += "/";
    topic += fromNodeHex;
    if (strlen(sensorData.sensor_id) > 0) {
        topic += "/";
        topic += sensorData.sensor_id;
    }

    // Construct Payload (JSON)
    // Estimate JSON size needed. Adjust capacity as needed.
    const int jsonCapacity = JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(sensorData.readings_count) + 256;
    StaticJsonDocument<jsonCapacity> doc;

    doc["node_id"] = fromNodeHex; // Originating node ID
    doc["sensor_id"] = sensorData.sensor_id;
    doc["timestamp_utc"] = sensorData.timestamp_utc;
    doc["sequence_num"] = sensorData.sequence_num;
    // Add Meshtastic metadata?
    // NodeInfo *ni = m_api->getNode(fromNode);
    // if (ni) {
    //     doc["rssi"] = ni->radio.rssi;
    //     doc["snr"] = ni->radio.snr;
    // }

    // Create a nested object for the readings map
    JsonObject readingsObj = doc.createNestedObject("readings");

    // ** PROBLEM REVISITED: Nanopb map decoding **
    // Just like encoding, decoding the map<string, float> requires callbacks.
    // Without them, sensorData.readings will be empty or unusable here.
    // TODO: Implement nanopb map decoding callbacks for SensorData.readings

    // ** WORKAROUND / Placeholder: **
    // If we assume fixed fields (temp, humidity, etc.) were used in the proto:
    // if (sensorData.has_temperature_c) readingsObj["temperature_c"] = sensorData.temperature_c;
    // if (sensorData.has_humidity_pct) readingsObj["humidity_pct"] = sensorData.humidity_pct;
    // if (sensorData.has_battery_v) readingsObj["battery_v"] = sensorData.battery_v;

     if (sensorData.readings_count == 0) {
         Log.printf(LOG_LEVEL_WARNING, "%s: SensorData readings map is empty (likely due to missing nanopb map callbacks).\n", getName());
         // Add a placeholder note to the JSON
         readingsObj["status"] = "Map data unavailable";
     } else {
          Log.printf(LOG_LEVEL_WARNING, "%s: SensorData readings map decoding not implemented. Cannot add readings to JSON.\n", getName());
          readingsObj["status"] = "Map decoding not implemented";
     }


    // Serialize JSON to string
    std::string payload;
    serializeJson(doc, payload);

    Log.printf(LOG_LEVEL_INFO, "%s: Publishing to MQTT topic: %s\n", getName(), topic.c_str());
    Log.printf(LOG_LEVEL_DEBUG, "%s: MQTT Payload: %s\n", getName(), payload.c_str());

    // Publish
    if (!m_mqttClient->publish(topic.c_str(), payload.c_str(), true)) { // Retain message
         Log.printf(LOG_LEVEL_ERROR, "%s: MQTT publish failed!\n", getName());
    }
}
#else
// Stub if Gateway role not compiled
void AkitaSmartCityServices::publishMqtt(const SensorData &, uint32_t) {
     Log.printf(LOG_LEVEL_WARNING, "%s: publishMqtt called, but Gateway role not compiled in.\n", getName());
}
#endif // ASCS_ROLE_GATEWAY
