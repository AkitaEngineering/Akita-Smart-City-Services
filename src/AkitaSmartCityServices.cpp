#include "AkitaSmartCityServices.h"
#include "meshtastic.h"
#include "plugin_api.h"
#include "pb_encode.h"
#include "pb_decode.h"
#include "mesh_packet.h"
#include "globals.h"

#ifdef ASCS_ROLE_GATEWAY
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#define FileSystem SPIFFS
#endif

#include "pb_common.h"
#include "pb.h"

AkitaSmartCityServices* AkitaSmartCityServices::s_instance = nullptr;

// --- Nanopb Callbacks ---

bool pb_encode_string_helper(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    const std::string* str = static_cast<const std::string*>(*arg);
    if (!str) return false;
    if (!pb_encode_tag_for_field(stream, field)) return false;
    return pb_encode_string(stream, reinterpret_cast<const pb_byte_t*>(str->c_str()), str->length());
}

bool pb_decode_string_helper(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    std::string* str = static_cast<std::string*>(*arg);
    if (!str) return false;
    size_t len = stream->bytes_left;
    try {
        str->resize(len);
    } catch (...) { return false; }
    if (!pb_read(stream, reinterpret_cast<pb_byte_t*>(&(*str)[0]), len)) return false;
    return true;
}

bool AkitaSmartCityServices::encode_map_callback(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    MapCallbackContext* context = static_cast<MapCallbackContext*>(*arg);
    if (!context || !context->map_ptr) return false;

    const std::map<std::string, float>& map_to_encode = *context->map_ptr;

    if (stream->state == nullptr) {
        context->map_iterator = map_to_encode.begin();
        stream->state = context;
        context->encode_successful = true;
    }

    while (context->encode_successful && context->map_iterator != map_to_encode.end()) {
        struct Entry { pb_callback_t key; float value; };

        Entry entry_data;
        entry_data.key.funcs.encode = pb_encode_string_helper;
        entry_data.key.arg = const_cast<void*>(static_cast<const void*>(&(context->map_iterator->first)));
        entry_data.value = context->map_iterator->second;

        /* Use the generated message descriptor for the ReadingsEntry submessage */
        if (!pb_encode_tag_for_field(stream, field) || !pb_encode_submessage(stream, SensorData_ReadingsEntry_fields, &entry_data)) {
            context->encode_successful = false;
            break;
        }
        ++(context->map_iterator);
    }

    if (context->map_iterator == map_to_encode.end()) stream->state = nullptr;
    return context->encode_successful;
}

bool AkitaSmartCityServices::decode_map_callback(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    MapCallbackContext* context = static_cast<MapCallbackContext*>(*arg);
    if (!context || !context->map_ptr) return false;

    struct Entry { pb_callback_t key; float value; };
    Entry entry_data;
    std::string current_key;
    entry_data.key.funcs.decode = pb_decode_string_helper;
    entry_data.key.arg = &current_key;
    entry_data.value = 0.0f;

    /* Use the generated message descriptor for decoding the ReadingsEntry */
    if (!pb_decode(stream, SensorData_ReadingsEntry_fields, &entry_data)) return false;
    (*context->map_ptr)[current_key] = entry_data.value;
    return true;
}

// --- Lifecycle ---

AkitaSmartCityServices::AkitaSmartCityServices(const char *name) : MeshtasticPlugin(name) {
    m_wifiClient = nullptr;
    m_mqttClient = nullptr;
    s_instance = this;
}

AkitaSmartCityServices::~AkitaSmartCityServices() {
#ifdef ASCS_ROLE_GATEWAY
    delete m_mqttClient;
    delete m_wifiClient;
#endif
    if (s_instance == this) s_instance = nullptr;
}

void AkitaSmartCityServices::init(const MeshtasticAPI *api) {
    m_api = api;
    m_config.load();
    Log.printf(LOG_LEVEL_INFO, "[%s] Init: Role=%d, SvcID=%lu\n", getName(), m_config.getNodeRole(), m_config.getServiceId());

    if (m_config.getNodeRole() == ServiceDiscovery_Role_GATEWAY) {
        #ifdef ASCS_ROLE_GATEWAY
            Log.println(LOG_LEVEL_INFO, "[%s] Init Gateway...", getName());
            m_wifiClient = new WiFiClient();
            m_mqttClient = new PubSubClient(*m_wifiClient);
            m_mqttClient->setServer(m_config.getMqttServer().c_str(), m_config.getMqttPort());
            m_mqttClient->setCallback(mqttCallback);
            m_mqttClient->setBufferSize(1024); // Critical: Increase buffer for JSON payloads

            if (!FileSystem.begin(true)) Log.println(LOG_LEVEL_ERROR, "[%s] FS Mount Failed!", getName());
            else Log.println(LOG_LEVEL_INFO, "[%s] FS Mounted.", getName());

            connectWiFi();
        #else
            Log.println(LOG_LEVEL_ERROR, "[%s] Gateway role set but ASCS_ROLE_GATEWAY not defined!", getName());
        #endif
    }
    sendServiceDiscovery();
}

void AkitaSmartCityServices::loop() {
    unsigned long now = millis();

    // Role Logic
    if (m_config.getNodeRole() == ServiceDiscovery_Role_SENSOR && m_sensor) {
        if (now - m_lastSensorReadTime >= m_config.getSensorReadIntervalMs()) {
            runSensorLogic();
            m_lastSensorReadTime = now;
        }
    } else if (m_config.getNodeRole() == ServiceDiscovery_Role_GATEWAY) {
        #ifdef ASCS_ROLE_GATEWAY
            checkWiFiConnection();
            checkMQTTConnection();
            if (m_mqttClient && m_mqttClient->connected()) {
                m_mqttClient->loop();
                if (now - m_lastBufferProcessTime > 2000) { // Check buffer frequently
                    processBufferedPackets();
                    m_lastBufferProcessTime = now;
                }
            }
        #endif
    }

    if (now - m_lastDiscoverySendTime >= m_config.getDiscoveryIntervalMs()) {
        sendServiceDiscovery();
        m_lastDiscoverySendTime = now;
    }
    if (now - m_lastServiceCleanupTime >= (m_config.getServiceTimeoutMs() / 2)) {
        cleanupServiceTable();
        m_lastServiceCleanupTime = now;
    }
}

bool AkitaSmartCityServices::handleReceived(const meshPacket *packet) {
    if (!packet) return false;
    if (packet->decoded.portnum != ASCS_PORT_NUM) return false;

    SmartCityPacket scp = SmartCityPacket_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(packet->decoded.payload, packet->decoded.payloadlen);

    // Prepare map for potential SensorData
    MapCallbackContext decode_context;
    std::map<std::string, float> decoded_readings;
    decode_context.map_ptr = &decoded_readings;
    scp.payload.sensor_data.readings.funcs.decode = decode_map_callback;
    scp.payload.sensor_data.readings.arg = &decode_context;

    if (pb_decode(&stream, SmartCityPacket_fields, &scp)) {
        switch (scp.which_payload) {
            case SmartCityPacket_discovery_tag:
                handleServiceDiscovery(scp.payload.discovery, packet->from);
                break;
            case SmartCityPacket_sensor_data_tag:
                // Pass the DECODED map to the handler
                Log.printf(LOG_LEVEL_DEBUG, "[%s] Rx SensorData from 0x%x, %d readings\n", getName(), packet->from, decoded_readings.size());
                handleSensorData(scp.payload.sensor_data, packet->from, &decoded_readings);
                break;
            default: break;
        }
        return true;
    }
    return false;
}

// --- Handlers ---

void AkitaSmartCityServices::handleSensorData(const SensorData &sensorData, uint32_t fromNode, const std::map<std::string, float>* readingsMap) {
    // If we are Aggregator, we need to re-wrap and send. 
    // If Gateway, we use data + map.
    
    switch (m_config.getNodeRole()) {
        case ServiceDiscovery_Role_AGGREGATOR: {
            // Re-construct packet for forwarding
            // Note: Aggregators currently just forward the raw packet bytes would be more efficient,
            // but for API consistency we re-encode.
            SmartCityPacket packet = SmartCityPacket_init_zero;
            packet.which_payload = SmartCityPacket_sensor_data_tag;
            packet.payload.sensor_data = sensorData;
            
            // We must re-attach callbacks if we are to re-encode the map
            MapCallbackContext encode_context;
            // Use const_cast to satisfy the struct, we won't modify it
            encode_context.map_ptr = const_cast<std::map<std::string, float>*>(readingsMap);
            packet.payload.sensor_data.readings.funcs.encode = encode_map_callback;
            packet.payload.sensor_data.readings.arg = &encode_context;

            runAggregatorLogic(packet, fromNode);
            break;
        }
        case ServiceDiscovery_Role_GATEWAY:
            runGatewayLogic(sensorData, fromNode, readingsMap);
            break;
        default: break;
    }
}

void AkitaSmartCityServices::runSensorLogic() {
    if (!m_sensor) return;
    std::map<std::string, float> readingsMap;
    if (m_sensor->readData(readingsMap)) {
        SensorData data = SensorData_init_zero;
        // sensor_id is a nanopb callback field; defer attaching the actual string
        // when encoding to ensure the string remains in scope.
        data.timestamp_utc = m_api->getAdjustedTime();
        data.sequence_num = ++m_sensorSequenceNum;
        sendSensorData(data, readingsMap, m_sensor->getSensorId());
    }
}

void AkitaSmartCityServices::runAggregatorLogic(const SmartCityPacket &packet, uint32_t fromNode) {
    uint32_t target = m_config.getTargetNodeId();
    if (target == 0) target = findGatewayNode();
    if (target != 0) sendMessage(target, packet);
}

void AkitaSmartCityServices::runGatewayLogic(const SensorData &sensorData, uint32_t fromNode, const std::map<std::string, float>* readingsMap) {
    #ifdef ASCS_ROLE_GATEWAY
    publishMqttOrBuffer(sensorData, fromNode, readingsMap);
    #endif
}

// --- Sending ---

void AkitaSmartCityServices::sendSensorData(const SensorData &sensorData, std::map<std::string, float>& readingsMap, const std::string &sensorId) {
    uint32_t target = m_config.getTargetNodeId();
    if (target == 0) target = findGatewayNode();
    if (target == 0) target = ASCS_BROADCAST_ADDR;

     SmartCityPacket packet = SmartCityPacket_init_zero;
    packet.which_payload = SmartCityPacket_sensor_data_tag;
    packet.payload.sensor_data = sensorData;

     /* If sensorData.sensor_id is callback-based we need a live string
         for the callback to reference while encoding; callers may pass
         sensorId via the new optional parameter. */
    std::string local_sensor_id = sensorId;

    if (!local_sensor_id.empty()) {
        packet.payload.sensor_data.sensor_id.funcs.encode = pb_encode_string_helper;
        packet.payload.sensor_data.sensor_id.arg = &local_sensor_id;
    }

    MapCallbackContext encode_context;
     encode_context.map_ptr = &readingsMap;
     packet.payload.sensor_data.readings.funcs.encode = encode_map_callback;
     packet.payload.sensor_data.readings.arg = &encode_context;

    sendMessage(target, packet);
}

bool AkitaSmartCityServices::sendMessage(uint32_t toNode, const SmartCityPacket &packet) {
    uint8_t buffer[ASCS_GATEWAY_MAX_PACKET_SIZE];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    if (pb_encode(&stream, SmartCityPacket_fields, &packet)) {
        MeshInterface *iface = m_api->getPrimaryInterface();
        if (iface) return iface->sendData(toNode, buffer, stream.bytes_written, ASCS_PORT_NUM, Data_WANT_ACK_DEFAULT, 0);
    }
    return false;
}

// --- Gateway Logic ---

#ifdef ASCS_ROLE_GATEWAY
void AkitaSmartCityServices::publishMqttOrBuffer(const SensorData &sensorData, uint32_t fromNode, const std::map<std::string, float>* readingsMap) {
    if (m_mqttClient && m_mqttClient->connected() && !m_gatewayBufferActive) {
        if (!publishMqtt(sensorData, fromNode, readingsMap)) {
            Log.println(LOG_LEVEL_WARNING, "[%s] Publish failed, buffering...", getName());
            m_gatewayBufferActive = true;
            bufferPacket(sensorData, fromNode, readingsMap);
        }
    } else {
        if (!m_gatewayBufferActive) m_gatewayBufferActive = true;
        bufferPacket(sensorData, fromNode, readingsMap);
    }
}

bool AkitaSmartCityServices::publishMqtt(const SensorData &sensorData, uint32_t fromNode, const std::map<std::string, float>* readingsMap) {
    if (!m_mqttClient || !m_mqttClient->connected()) return false;

    // Estimate capacity: Base + Map Size
    size_t mapSize = readingsMap ? readingsMap->size() : 0;
    size_t capacity = JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(mapSize) + (mapSize * 64) + 256;
    DynamicJsonDocument doc(capacity); // Use Dynamic for flexible sizing

    char nodeHex[9]; snprintf(nodeHex, sizeof(nodeHex), "%08lx", fromNode);
    doc["node_id"] = nodeHex;
    doc["sensor_id"] = sensorData.sensor_id;
    doc["timestamp_utc"] = sensorData.timestamp_utc;
    doc["sequence_num"] = sensorData.sequence_num;

    JsonObject readings = doc.createNestedObject("readings");
    if (readingsMap) {
        for (const auto& pair : *readingsMap) {
            readings[pair.first] = pair.second;
        }
    } else {
        readings["error"] = "No map data";
    }

    std::string topic = m_config.getMqttBaseTopic() + "/sensor/" + std::to_string(m_config.getServiceId()) + "/" + nodeHex;
    if (strlen(sensorData.sensor_id) > 0) {
        topic += "/";
        topic += sensorData.sensor_id;
    }

    std::string payload;
    serializeJson(doc, payload);
    return m_mqttClient->publish(topic.c_str(), payload.c_str(), false);
}

void AkitaSmartCityServices::bufferPacket(const SensorData &sensorData, uint32_t fromNode, const std::map<std::string, float>* readingsMap) {
    // We need to store FROM_NODE + PACKET_LEN + PACKET
    
    // 1. Re-encode the packet to bytes
    SmartCityPacket packet = SmartCityPacket_init_zero;
    packet.which_payload = SmartCityPacket_sensor_data_tag;
    packet.payload.sensor_data = sensorData;
    
    MapCallbackContext encode_context;
    // const_cast safe here as we only read
    encode_context.map_ptr = const_cast<std::map<std::string, float>*>(readingsMap);
    packet.payload.sensor_data.readings.funcs.encode = encode_map_callback;
    packet.payload.sensor_data.readings.arg = &encode_context;

    uint8_t buffer[ASCS_GATEWAY_MAX_PACKET_SIZE];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    
    if (!pb_encode(&stream, SmartCityPacket_fields, &packet)) {
        Log.println(LOG_LEVEL_ERROR, "[%s] Buffer encode failed", getName());
        return;
    }
    size_t pktLen = stream.bytes_written;

    File file = FileSystem.open(ASCS_GATEWAY_BUFFER_FILENAME, FILE_APPEND);
    if (!file) return;

    // Format: [FromNode(4)][Length(2)][PacketData(n)]
    uint16_t len16 = (uint16_t)pktLen;
    file.write((uint8_t*)&fromNode, sizeof(fromNode));
    file.write((uint8_t*)&len16, sizeof(len16));
    file.write(buffer, pktLen);
    file.close();
    
    Log.printf(LOG_LEVEL_INFO, "[%s] Buffered %d bytes from 0x%x\n", getName(), pktLen, fromNode);
}

bool AkitaSmartCityServices::readPacketFromBuffer(File &file, uint8_t* buffer, size_t &len, uint32_t &fromNode) {
    if (file.available() < sizeof(uint32_t) + sizeof(uint16_t)) return false;

    file.read((uint8_t*)&fromNode, sizeof(fromNode));
    
    uint16_t pktLen;
    file.read((uint8_t*)&pktLen, sizeof(pktLen));

    if (pktLen > ASCS_GATEWAY_MAX_PACKET_SIZE || file.available() < pktLen) return false;

    file.read(buffer, pktLen);
    len = pktLen;
    return true;
}

void AkitaSmartCityServices::removePacketFromBuffer() {
    File r = FileSystem.open(ASCS_GATEWAY_BUFFER_FILENAME, FILE_READ);
    if (!r) return;

    // Read header to find size to skip
    uint32_t fromNode;
    uint16_t pktLen;
    if (r.read((uint8_t*)&fromNode, 4) != 4 || r.read((uint8_t*)&pktLen, 2) != 2) {
        r.close(); FileSystem.remove(ASCS_GATEWAY_BUFFER_FILENAME); return;
    }

    size_t skip = 6 + pktLen;
    size_t total = r.size();

    if (skip >= total) {
        r.close(); FileSystem.remove(ASCS_GATEWAY_BUFFER_FILENAME); return;
    }

    File w = FileSystem.open("/ascs_tmp.dat", FILE_WRITE);
    r.seek(skip);
    
    uint8_t buf[256];
    while(r.available()) {
        size_t n = r.read(buf, sizeof(buf));
        w.write(buf, n);
    }
    r.close(); w.close();
    FileSystem.remove(ASCS_GATEWAY_BUFFER_FILENAME);
    FileSystem.rename("/ascs_tmp.dat", ASCS_GATEWAY_BUFFER_FILENAME);
}

void AkitaSmartCityServices::processBufferedPackets() {
    File f = FileSystem.open(ASCS_GATEWAY_BUFFER_FILENAME, FILE_READ);
    if (!f || f.size() == 0) { 
        m_gatewayBufferActive = false; 
        if(f) f.close(); 
        return; 
    }

    uint8_t buf[ASCS_GATEWAY_MAX_PACKET_SIZE];
    size_t len;
    uint32_t fromNode;

    if (readPacketFromBuffer(f, buf, len, fromNode)) {
        f.close();
        
        SmartCityPacket scp = SmartCityPacket_init_zero;
        pb_istream_t stream = pb_istream_from_buffer(buf, len);
        
        MapCallbackContext decode_context;
        std::map<std::string, float> decoded_readings;
        decode_context.map_ptr = &decoded_readings;
        scp.payload.sensor_data.readings.funcs.decode = decode_map_callback;
        scp.payload.sensor_data.readings.arg = &decode_context;

        if (pb_decode(&stream, SmartCityPacket_fields, &scp) && scp.which_payload == SmartCityPacket_sensor_data_tag) {
            if (publishMqtt(scp.payload.sensor_data, fromNode, &decoded_readings)) {
                removePacketFromBuffer();
                m_lastBufferProcessTime = millis(); // Reset timer to process next quickly
            }
        } else {
            // Corrupt, remove
            removePacketFromBuffer();
        }
    } else {
        f.close();
        FileSystem.remove(ASCS_GATEWAY_BUFFER_FILENAME); // Corrupt file
    }
}
#endif

// --- Misc Stubs ---
void AkitaSmartCityServices::setSensor(std::unique_ptr<SensorInterface> sensor) { m_sensor = std::move(sensor); }
ServiceDiscovery_Role AkitaSmartCityServices::getNodeRole() const { return m_config.getNodeRole(); }
void AkitaSmartCityServices::handleServiceDiscovery(const ServiceDiscovery &discovery, uint32_t fromNode) { updateServiceTable(fromNode, discovery.node_role, discovery.service_id); }
void AkitaSmartCityServices::updateServiceTable(uint32_t nodeId, ServiceDiscovery_Role role, uint32_t serviceId) { if(nodeId != m_api->getMyNodeInfo()->node_num) m_serviceTable[nodeId] = {role, serviceId, millis()}; }
void AkitaSmartCityServices::cleanupServiceTable() {
    unsigned long now = millis();
    for (auto it = m_serviceTable.begin(); it != m_serviceTable.end(); ) {
        if (now - it->second.lastSeen > m_config.getServiceTimeoutMs()) {
            it = m_serviceTable.erase(it);
        } else {
            ++it;
        }
    }
}
uint32_t AkitaSmartCityServices::findGatewayNode() {
    for (const auto& entry : m_serviceTable) {
        if (entry.second.role == ServiceDiscovery_Role_GATEWAY) {
            return entry.first;
        }
    }
    return 0; // No gateway found
}
void AkitaSmartCityServices::sendServiceDiscovery(uint32_t to) { 
    SmartCityPacket p = SmartCityPacket_init_zero; 
    p.which_payload = SmartCityPacket_discovery_tag; 
    p.payload.discovery.node_role = m_config.getNodeRole();
    p.payload.discovery.service_id = m_config.getServiceId();
    sendMessage(to, p); 
}
#ifdef ASCS_ROLE_GATEWAY
void AkitaSmartCityServices::connectWiFi() {
    if (!m_wifiClient) return;
    const std::string ssid = m_config.getWifiSsid();
    const std::string pass = m_config.getWifiPassword();
    if (WiFi.status() != WL_CONNECTED) {
        Log.printf(LOG_LEVEL_INFO, "[%s] Connecting to WiFi SSID='%s'...\n", getName(), ssid.c_str());
        WiFi.begin(ssid.c_str(), pass.c_str());
        m_lastMqttReconnectAttempt = millis();
    }
}

void AkitaSmartCityServices::checkWiFiConnection() {
    if (WiFi.status() == WL_CONNECTED) return;
    unsigned long now = millis();
    // Try to reconnect every mqtt reconnect interval (configurable)
    if (now - m_lastMqttReconnectAttempt >= m_config.getMqttReconnectIntervalMs()) {
        m_lastMqttReconnectAttempt = now;
        Log.printf(LOG_LEVEL_INFO, "[%s] WiFi disconnected, attempting reconnect...\n", getName());
        WiFi.disconnect();
        WiFi.begin(m_config.getWifiSsid().c_str(), m_config.getWifiPassword().c_str());
    }
}

void AkitaSmartCityServices::connectMQTT() {
    if (!m_mqttClient) return;
    if (m_mqttClient->connected()) return;

    if (WiFi.status() != WL_CONNECTED) {
        Log.println(LOG_LEVEL_DEBUG, "[ASCS] Skipping MQTT connect: WiFi not connected");
        return;
    }

    char clientId[48];
    uint32_t svc = m_config.getServiceId();
    snprintf(clientId, sizeof(clientId), "ascs_gw_%lu", (unsigned long)svc);

    Log.printf(LOG_LEVEL_INFO, "[%s] Connecting to MQTT broker %s:%d as %s\n", getName(), m_config.getMqttServer().c_str(), m_config.getMqttPort(), clientId);

    bool ok;
    const std::string user = m_config.getMqttUser();
    const std::string pass = m_config.getMqttPassword();

    if (user.empty()) {
        ok = m_mqttClient->connect(clientId);
    } else {
        ok = m_mqttClient->connect(clientId, user.c_str(), pass.c_str());
    }

    if (ok) {
        Log.printf(LOG_LEVEL_INFO, "[%s] MQTT connected.\n", getName());
    } else {
        Log.printf(LOG_LEVEL_WARNING, "[%s] MQTT connect failed\n", getName());
    }
}

void AkitaSmartCityServices::checkMQTTConnection() {
    if (!m_mqttClient) return;
    if (m_mqttClient->connected()) return;

    unsigned long now = millis();
    if (now - m_lastMqttReconnectAttempt >= m_config.getMqttReconnectIntervalMs()) {
        m_lastMqttReconnectAttempt = now;
        connectMQTT();
    }
}

void AkitaSmartCityServices::mqttCallback(char *topic, byte *payload, unsigned int length) {
    // Simple handler: forward to instance if available
    if (!AkitaSmartCityServices::s_instance) return;
    std::string t(topic ? topic : "");
    std::string p;
    if (payload && length > 0) p.assign(reinterpret_cast<char*>(payload), length);

    Log.printf(LOG_LEVEL_INFO, "[%s] MQTT Msg on %s: %s\n", AkitaSmartCityServices::s_instance->getName(), t.c_str(), p.c_str());
}
#else
void AkitaSmartCityServices::connectWiFi() {}
void AkitaSmartCityServices::checkWiFiConnection() {}
void AkitaSmartCityServices::connectMQTT() {}
void AkitaSmartCityServices::checkMQTTConnection() {}
void AkitaSmartCityServices::mqttCallback(char*, byte*, unsigned int) {}
#endif
