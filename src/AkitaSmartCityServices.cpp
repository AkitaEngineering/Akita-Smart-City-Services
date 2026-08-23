#include "AkitaSmartCityServices.h"
#include "MeshService.h"
#include "mesh/NodeDB.h"
#include "gps/RTC.h"
#include "DebugConfiguration.h"
#include "pb_encode.h"
#include "pb_decode.h"
#include "concurrency/LockGuard.h"

#ifdef ASCS_ROLE_GATEWAY
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#define FileSystem LittleFS
#endif

#include "pb_common.h"
#include "pb.h"
#include <cmath>
#include <cctype>
#ifdef ASCS_SENSOR_BME280
#include "sensors/BME280Sensor.h"
#endif

AkitaSmartCityServices* AkitaSmartCityServices::s_instance = nullptr;

namespace {
bool isSafeIdentifier(const char *value, size_t maximumLength) {
    if (!value) return false;
    const size_t length = strlen(value);
    if (length == 0 || length > maximumLength) return false;
    for (size_t i = 0; i < length; ++i) {
        const unsigned char character = static_cast<unsigned char>(value[i]);
        if (!std::isalnum(character) && character != '-' && character != '_' && character != '.') return false;
    }
    return true;
}

bool isUuid(const char *value) {
    if (!value || strlen(value) != 36) return false;
    for (size_t i = 0; i < 36; ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (value[i] != '-') return false;
        } else if (!std::isxdigit(static_cast<unsigned char>(value[i]))) {
            return false;
        }
    }
    const char version = static_cast<char>(std::tolower(static_cast<unsigned char>(value[14])));
    const char variant = static_cast<char>(std::tolower(static_cast<unsigned char>(value[19])));
    return version >= '1' && version <= '8' && (variant == '8' || variant == '9' || variant == 'a' || variant == 'b');
}

bool isSafeAction(const char *value) {
    if (!value) return false;
    const size_t length = strlen(value);
    if (length == 0 || length > 32) return false;
    if (!std::isalpha(static_cast<unsigned char>(value[0]))) return false;
    for (size_t i = 0; i < length; ++i) {
        const unsigned char character = static_cast<unsigned char>(value[i]);
        if (!std::isalnum(character) && character != '-' && character != '_') return false;
    }
    return true;
}

#ifdef ASCS_ROLE_GATEWAY
bool isLowerHexNodeId(const std::string &value) {
    if (value.size() != 8) return false;
    for (const unsigned char character : value) {
        if (!std::isdigit(character) && (character < 'a' || character > 'f')) return false;
    }
    return true;
}

bool isIsoUtcTimestamp(const char *value) {
    if (!value || strlen(value) != 24 || value[4] != '-' || value[7] != '-' || value[10] != 'T' ||
        value[13] != ':' || value[16] != ':' || value[19] != '.' || value[23] != 'Z') return false;
    constexpr size_t numericPositions[] = {0, 1, 2, 3, 5, 6, 8, 9, 11, 12, 14, 15, 17, 18, 20, 21, 22};
    for (const size_t position : numericPositions) {
        if (!std::isdigit(static_cast<unsigned char>(value[position]))) return false;
    }
    const auto pair = [value](size_t position) {
        return static_cast<unsigned>((value[position] - '0') * 10 + (value[position + 1] - '0'));
    };
    const unsigned year = static_cast<unsigned>((value[0] - '0') * 1000 + (value[1] - '0') * 100 +
                                                (value[2] - '0') * 10 + (value[3] - '0'));
    const unsigned month = pair(5);
    const unsigned day = pair(8);
    if (year == 0 || month < 1 || month > 12) return false;
    constexpr unsigned daysByMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    unsigned daysInMonth = daysByMonth[month - 1];
    if (month == 2 && (year % 4 == 0) && (year % 100 != 0 || year % 400 == 0)) daysInMonth = 29;
    return day >= 1 && day <= daysInMonth && pair(11) <= 23 && pair(14) <= 59 && pair(17) <= 59;
}

bool isValidRequester(const char *value) {
    if (!value) return false;
    const size_t length = strlen(value);
    if (length == 0 || length > 128) return false;
    for (size_t index = 0; index < length; ++index) {
        if (std::iscntrl(static_cast<unsigned char>(value[index]))) return false;
    }
    return true;
}

uint32_t bufferRecordCrc(uint32_t fromNode, const uint8_t *payload, size_t length) {
    uint32_t crc = 0xFFFFFFFFUL;
    const auto update = [&crc](uint8_t byte) {
        crc ^= byte;
        for (uint8_t bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1UL)));
    };
    for (size_t index = 0; index < sizeof(fromNode); ++index) {
        update(static_cast<uint8_t>((fromNode >> (index * 8)) & 0xFFU));
    }
    for (size_t index = 0; index < length; ++index) update(payload[index]);
    return ~crc;
}
#endif
}

// --- Nanopb Callbacks ---

bool pb_encode_string_helper(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    const std::string* str = static_cast<const std::string*>(*arg);
    if (!str) return false;
    if (!pb_encode_tag_for_field(stream, field)) return false;
    return pb_encode_string(stream, reinterpret_cast<const pb_byte_t*>(str->c_str()), str->length());
}

bool pb_decode_string_helper(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    (void)field;
    std::string* str = static_cast<std::string*>(*arg);
    if (!str) return false;
    const size_t len = stream->bytes_left;
    if (len == 0 || len > ASCS_MAX_READING_KEY_LENGTH) return false;
    try {
        str->resize(len);
    } catch (...) { return false; }
    return pb_read(stream, reinterpret_cast<pb_byte_t*>(str->data()), len);
}

bool AkitaSmartCityServices::encode_map_callback(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    MapCallbackContext* context = static_cast<MapCallbackContext*>(*arg);
    if (!context || !context->map_ptr || context->map_ptr->size() > ASCS_MAX_READINGS) return false;

    const std::map<std::string, float>& map_to_encode = *context->map_ptr;
    for (const auto &reading : map_to_encode) {
        if (!isSafeIdentifier(reading.first.c_str(), ASCS_MAX_READING_KEY_LENGTH) || !std::isfinite(reading.second)) {
            return false;
        }
        akita_smart_city_SensorData_ReadingsEntry entry_data = akita_smart_city_SensorData_ReadingsEntry_init_zero;
        entry_data.key.funcs.encode = pb_encode_string_helper;
        entry_data.key.arg = const_cast<void*>(static_cast<const void*>(&reading.first));
        entry_data.value = reading.second;

        if (!pb_encode_tag_for_field(stream, field) || !pb_encode_submessage(stream, SensorData_ReadingsEntry_fields, &entry_data)) {
            return false;
        }
    }
    return true;
}

bool AkitaSmartCityServices::decode_map_callback(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    (void)field;
    MapCallbackContext* context = static_cast<MapCallbackContext*>(*arg);
    if (!context || !context->map_ptr || context->decoded_entries >= ASCS_MAX_READINGS) return false;

    akita_smart_city_SensorData_ReadingsEntry entry_data = akita_smart_city_SensorData_ReadingsEntry_init_zero;
    std::string current_key;
    entry_data.key.funcs.decode = pb_decode_string_helper;
    entry_data.key.arg = &current_key;
    entry_data.value = 0.0f;

    /* Use the generated message descriptor for decoding the ReadingsEntry */
    if (!pb_decode(stream, SensorData_ReadingsEntry_fields, &entry_data) ||
        !isSafeIdentifier(current_key.c_str(), ASCS_MAX_READING_KEY_LENGTH) || !std::isfinite(entry_data.value)) {
        return false;
    }
    if (!context->map_ptr->emplace(current_key, entry_data.value).second) return false;
    ++context->decoded_entries;
    return true;
}

// --- Lifecycle ---

AkitaSmartCityServices::AkitaSmartCityServices(const char *name)
    : SinglePortModule(name, ASCS_PORT_NUM), concurrency::OSThread("ASCS") {
    m_wifiClient = nullptr;
    m_mqttClient = nullptr;
#ifdef ASCS_SENSOR_BME280
    m_sensor = std::make_unique<ASCSBME280Sensor>();
#endif
    s_instance = this;
}

AkitaSmartCityServices::~AkitaSmartCityServices() {
#ifdef ASCS_ROLE_GATEWAY
    delete m_mqttClient;
    delete m_wifiClient;
#endif
    if (s_instance == this) s_instance = nullptr;
}

void AkitaSmartCityServices::setup() { initialize(); }
int32_t AkitaSmartCityServices::runOnce() { loop(); return 100; }

void AkitaSmartCityServices::initialize() {
    m_config.load();
    if (!m_config.isValid()) {
        LOG_ERROR("[%s] Invalid configuration: %s\n", getName(), m_config.getValidationError().c_str());
        return;
    }
    LOG_INFO("[%s] Init: Role=%d, SvcID=%lu\n", getName(), m_config.getNodeRole(), m_config.getServiceId());

    if (m_config.getNodeRole() == ServiceDiscovery_Role_SENSOR) {
        if (!m_sensor || !m_sensor->initialize()) {
            LOG_ERROR("[%s] Sensor role requires an initialized SensorInterface implementation.", getName());
            return;
        }
        if (m_actuator && m_config.getTrustedGatewayNodeId() == 0) {
            LOG_ERROR("[%s] An actuator requires a nonzero trusted gateway node ID.", getName());
            return;
        }
        if (m_actuator) {
            char expectedAssetId[9];
            snprintf(expectedAssetId, sizeof(expectedAssetId), "%08lx", static_cast<unsigned long>(getNodeNumber()));
            if (getNodeNumber() == 0 || m_actuator->getAssetId() != expectedAssetId) {
                LOG_ERROR("[%s] Actuator asset ID must equal this device's lowercase eight-digit mesh node ID.", getName());
                return;
            }
        }
    }

    if (m_config.getNodeRole() == ServiceDiscovery_Role_GATEWAY) {
        #ifdef ASCS_ROLE_GATEWAY
            LOG_INFO("[%s] Init Gateway...", getName());
            m_wifiClient = new WiFiClientSecure();
            m_wifiClient->setCACert(m_config.getMqttCaCert().c_str());
            m_mqttClient = new PubSubClient(*m_wifiClient);
            m_mqttClient->setServer(m_config.getMqttServer().c_str(), m_config.getMqttPort());
            m_mqttClient->setCallback(mqttCallback);
            if (!m_mqttClient->setBufferSize(ASCS_MQTT_BUFFER_SIZE, ASCS_MQTT_BUFFER_SIZE)) {
                LOG_ERROR("[%s] MQTT buffers could not be allocated.", getName());
                return;
            }

            m_fileSystemReady = FileSystem.begin(false);
            if (!m_fileSystemReady) LOG_ERROR("[%s] LittleFS mount failed; offline telemetry buffering is disabled.", getName());
            else {
                if (!recoverBufferFile()) {
                    LOG_ERROR("[%s] Interrupted telemetry queue recovery failed; offline buffering is disabled.", getName());
                    m_fileSystemReady = false;
                    return;
                }
                File queue = FileSystem.open(ASCS_GATEWAY_BUFFER_FILENAME, FILE_READ);
                m_gatewayBufferActive = queue && queue.size() > 0;
                if (queue) queue.close();
                LOG_INFO("[%s] LittleFS mounted.", getName());
            }

            connectWiFi();
        #else
            LOG_ERROR("[%s] Gateway role set but ASCS_ROLE_GATEWAY not defined!", getName());
            return;
        #endif
    }
    m_initialized = true;
    sendServiceDiscovery();
}

void AkitaSmartCityServices::loop() {
    if (!m_initialized) return;
    unsigned long now = millis();

    // Role Logic
    if (m_config.getNodeRole() == ServiceDiscovery_Role_SENSOR && m_sensor) {
        if (now - m_lastSensorReadTime >= m_config.getSensorReadIntervalMs()) {
            runSensorLogic();
            m_lastSensorReadTime = now;
        }
    } else if (m_config.getNodeRole() == ServiceDiscovery_Role_GATEWAY) {
        #ifdef ASCS_ROLE_GATEWAY
            concurrency::LockGuard gatewayGuard(&m_gatewayLock);
            checkWiFiConnection();
            checkMQTTConnection();
            if (m_mqttClient && m_mqttClient->connected()) {
                m_mqttClient->loop();
                processPendingControlCommands();
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

ProcessMessage AkitaSmartCityServices::handleReceived(const meshtastic_MeshPacket &packet) {
    if (!m_initialized || packet.which_payload_variant != meshtastic_MeshPacket_decoded_tag ||
        packet.decoded.portnum != ASCS_PORT_NUM) return ProcessMessage::CONTINUE;
    return handlePayload(packet.decoded.payload.bytes, packet.decoded.payload.size, packet.from)
        ? ProcessMessage::STOP : ProcessMessage::CONTINUE;
}

bool AkitaSmartCityServices::handlePayload(const uint8_t *payload, size_t payloadLength, uint32_t fromNode) {
    if (!payload || payloadLength == 0 || payloadLength > ASCS_GATEWAY_MAX_PACKET_SIZE) return false;
    SmartCityPacket scp = SmartCityPacket_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(payload, payloadLength);

    // Prepare map for potential SensorData
    MapCallbackContext decode_context;
    std::map<std::string, float> decoded_readings;
    decode_context.map_ptr = &decoded_readings;
    scp.payload.sensor_data.readings.funcs.decode = decode_map_callback;
    scp.payload.sensor_data.readings.arg = &decode_context;

    if (pb_decode(&stream, SmartCityPacket_fields, &scp)) {
        switch (scp.which_payload) {
            case SmartCityPacket_discovery_tag:
                handleServiceDiscovery(scp.payload.discovery, fromNode);
                break;
            case SmartCityPacket_sensor_data_tag:
                if (!isSafeIdentifier(scp.payload.sensor_data.sensor_id, 64) || decoded_readings.empty() ||
                    scp.payload.sensor_data.timestamp_utc == 0 || scp.payload.sensor_data.sequence_num == 0 ||
                    scp.payload.sensor_data.origin_node == 0 ||
                    !isTelemetryRouteAllowed(scp.payload.sensor_data.origin_node, fromNode)) return false;
                LOG_DEBUG("[%s] Rx SensorData from 0x%x, %d readings\n", getName(), fromNode, decoded_readings.size());
                handleSensorData(scp.payload.sensor_data, fromNode, &decoded_readings, scp.payload.sensor_data.sensor_id);
                break;
            case SmartCityPacket_control_command_tag:
                handleControlCommand(scp.payload.control_command, fromNode);
                break;
            case SmartCityPacket_control_ack_tag:
                handleControlAck(scp.payload.control_ack, fromNode);
                break;
            default: break;
        }
        return true;
    }
    return false;
}

// --- Handlers ---

void AkitaSmartCityServices::handleSensorData(const SensorData &sensorData, uint32_t fromNode, const std::map<std::string, float>* readingsMap, const std::string& decodedSensorId) {
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
            runGatewayLogic(sensorData, sensorData.origin_node, readingsMap, decodedSensorId);
            break;
        default: break;
    }
}

void AkitaSmartCityServices::runSensorLogic() {
    if (!m_sensor) return;
    std::map<std::string, float> readingsMap;
    if (m_sensor->readData(readingsMap)) {
        const uint32_t currentTime = getCurrentTime();
        const uint32_t nodeNumber = getNodeNumber();
        if (currentTime == 0 || nodeNumber == 0) {
            LOG_WARN("[%s] Sensor reading withheld until node identity and trusted time are available.", getName());
            return;
        }
        SensorData data = SensorData_init_zero;
        // sensor_id is a nanopb callback field; defer attaching the actual string
        // when encoding to ensure the string remains in scope.
        data.timestamp_utc = currentTime;
        if (++m_sensorSequenceNum == 0) ++m_sensorSequenceNum;
        data.sequence_num = m_sensorSequenceNum;
        data.origin_node = nodeNumber;
        sendSensorData(data, readingsMap, m_sensor->getSensorId());
    }
}

void AkitaSmartCityServices::handleControlCommand(const ControlCommand &command, uint32_t fromNode) {
    if (command.target_node != getNodeNumber()) return;
    if (!isUuid(command.command_id)) return;
    concurrency::LockGuard stateGuard(&m_stateLock);
    const auto gateway = m_serviceTable.find(fromNode);
    if (fromNode != m_config.getTrustedGatewayNodeId() || gateway == m_serviceTable.end() ||
        gateway->second.role != ServiceDiscovery_Role_GATEWAY || gateway->second.serviceId != m_config.getServiceId()) {
        sendControlAck(fromNode, command.command_id, ControlAck_Status_REJECTED, "Sender is not the trusted gateway");
        return;
    }
    if (!isSafeIdentifier(command.asset_id, 64) || !isSafeAction(command.action)) {
        sendControlAck(fromNode, command.command_id, ControlAck_Status_REJECTED, "Command fields are invalid");
        return;
    }
    if (!m_actuator || m_actuator->getAssetId() != command.asset_id) {
        sendControlAck(fromNode, command.command_id, ControlAck_Status_REJECTED, "Asset is not available on this node");
        return;
    }

    const auto cached = m_recentCommands.find(command.command_id);
    if (cached != m_recentCommands.end()) {
        sendControlAck(fromNode, command.command_id, cached->second.status, cached->second.detail);
        return;
    }

    const uint32_t currentTime = getCurrentTime();
    if (currentTime == 0 || command.expires_at_utc == 0 || currentTime > command.expires_at_utc) {
        sendControlAck(fromNode, command.command_id, ControlAck_Status_REJECTED, "Command expired or device clock is unavailable");
        return;
    }

    const bool isNumeric = command.which_value == ControlCommand_numeric_value_tag;
    if (!isNumeric && command.which_value != ControlCommand_bool_value_tag) {
        sendControlAck(fromNode, command.command_id, ControlAck_Status_REJECTED, "Command value is missing");
        return;
    }
    if (isNumeric && (!std::isfinite(command.value.numeric_value) ||
                      std::fabs(command.value.numeric_value) > ASCS_MAX_NUMERIC_COMMAND_VALUE)) {
        sendControlAck(fromNode, command.command_id, ControlAck_Status_REJECTED, "Numeric command value is outside the safe protocol range");
        return;
    }
    std::string detail;
    const bool executed = m_actuator->execute(command.action, isNumeric, command.value.numeric_value,
                                              command.value.bool_value, detail);
    if (detail.empty()) detail = executed ? "Executed" : "Actuator rejected the command";
    if (detail.size() > 96) detail.resize(96);
    const ControlAck_Status status = executed ? ControlAck_Status_EXECUTED : ControlAck_Status_FAILED;
    m_recentCommands[command.command_id] = {status, detail, millis()};
    if (m_recentCommands.size() > 32) {
        auto oldest = m_recentCommands.begin();
        for (auto it = m_recentCommands.begin(); it != m_recentCommands.end(); ++it) {
            if (millis() - it->second.completedAt > millis() - oldest->second.completedAt) oldest = it;
        }
        m_recentCommands.erase(oldest);
    }
    sendControlAck(fromNode, command.command_id, status, detail);
}

void AkitaSmartCityServices::handleControlAck(const ControlAck &ack, uint32_t fromNode) {
#ifdef ASCS_ROLE_GATEWAY
    if (m_config.getNodeRole() == ServiceDiscovery_Role_GATEWAY) {
        concurrency::LockGuard gatewayGuard(&m_gatewayLock);
        if (!isUuid(ack.command_id) || ack.status < ControlAck_Status_REJECTED || ack.status > ControlAck_Status_FAILED) return;
        const auto pending = m_pendingGatewayCommands.find(ack.command_id);
        if (pending == m_pendingGatewayCommands.end() || pending->second.targetNode != fromNode) return;
        pending->second.acknowledgement = ack;
        pending->second.acknowledged = true;
        if (publishControlAckMqtt(ack, fromNode)) m_pendingGatewayCommands.erase(pending);
    }
#else
    (void)ack;
    (void)fromNode;
#endif
}

void AkitaSmartCityServices::sendControlAck(uint32_t toNode, const char *commandId, ControlAck_Status status,
                                            const std::string &detail) {
    SmartCityPacket packet = SmartCityPacket_init_zero;
    packet.which_payload = SmartCityPacket_control_ack_tag;
    snprintf(packet.payload.control_ack.command_id, sizeof(packet.payload.control_ack.command_id), "%s", commandId);
    packet.payload.control_ack.status = status;
    snprintf(packet.payload.control_ack.detail, sizeof(packet.payload.control_ack.detail), "%s", detail.c_str());
    sendMessage(toNode, packet);
}

void AkitaSmartCityServices::runAggregatorLogic(const SmartCityPacket &packet, uint32_t fromNode) {
    uint32_t target = m_config.getTargetNodeId();
    if (target == 0) target = findGatewayNode();
    if (target != 0) sendMessage(target, packet);
}

void AkitaSmartCityServices::runGatewayLogic(const SensorData &sensorData, uint32_t fromNode, const std::map<std::string, float>* readingsMap, const std::string& decodedSensorId) {
    #ifdef ASCS_ROLE_GATEWAY
    concurrency::LockGuard gatewayGuard(&m_gatewayLock);
    publishMqttOrBuffer(sensorData, fromNode, readingsMap, decodedSensorId);
    #endif
}

// --- Sending ---

void AkitaSmartCityServices::sendSensorData(const SensorData &sensorData, std::map<std::string, float>& readingsMap, const std::string &sensorId) {
    if (!isSafeIdentifier(sensorId.c_str(), 64) || readingsMap.empty() || readingsMap.size() > ASCS_MAX_READINGS) {
        LOG_ERROR("[%s] Sensor data contains an invalid identifier or reading set.", getName());
        return;
    }
    uint32_t target = m_config.getTargetNodeId();
    if (target == 0) target = findGatewayNode();
    if (target == 0) target = ASCS_BROADCAST_ADDR;

     SmartCityPacket packet = SmartCityPacket_init_zero;
    packet.which_payload = SmartCityPacket_sensor_data_tag;
    packet.payload.sensor_data = sensorData;

    snprintf(packet.payload.sensor_data.sensor_id, sizeof(packet.payload.sensor_data.sensor_id), "%s", sensorId.c_str());

    MapCallbackContext encode_context;
     encode_context.map_ptr = &readingsMap;
     packet.payload.sensor_data.readings.funcs.encode = encode_map_callback;
     packet.payload.sensor_data.readings.arg = &encode_context;

    sendMessage(target, packet);
}

bool AkitaSmartCityServices::sendMessage(uint32_t toNode, const SmartCityPacket &packet) {
    if (!m_initialized) return false;
    uint8_t buffer[ASCS_GATEWAY_MAX_PACKET_SIZE];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    if (pb_encode(&stream, SmartCityPacket_fields, &packet)) {
        if (stream.bytes_written > meshtastic_Constants_DATA_PAYLOAD_LEN) return false;
        meshtastic_MeshPacket *outbound = allocDataPacket();
        if (!outbound) return false;
        outbound->to = toNode;
        outbound->want_ack = toNode != ASCS_BROADCAST_ADDR;
        outbound->decoded.payload.size = stream.bytes_written;
        memcpy(outbound->decoded.payload.bytes, buffer, stream.bytes_written);
        service->sendToMesh(outbound, RX_SRC_LOCAL, true);
        return true;
    }
    return false;
}

// --- Gateway Logic ---

#ifdef ASCS_ROLE_GATEWAY
void AkitaSmartCityServices::publishMqttOrBuffer(const SensorData &sensorData, uint32_t fromNode, const std::map<std::string, float>* readingsMap, const std::string& decodedSensorId) {
    if (m_mqttClient && m_mqttClient->connected() && !m_gatewayBufferActive) {
        if (!publishMqtt(sensorData, fromNode, readingsMap, decodedSensorId)) {
            LOG_WARN("[%s] Publish failed, buffering...", getName());
            m_gatewayBufferActive = true;
            bufferPacket(sensorData, fromNode, readingsMap, decodedSensorId);
        }
    } else {
        if (m_fileSystemReady) {
            m_gatewayBufferActive = true;
            bufferPacket(sensorData, fromNode, readingsMap, decodedSensorId);
        } else {
            m_gatewayBufferActive = false;
            LOG_ERROR("[%s] Telemetry dropped: MQTT and persistent buffering are unavailable.", getName());
        }
    }
}

bool AkitaSmartCityServices::publishMqtt(const SensorData &sensorData, uint32_t fromNode, const std::map<std::string, float>* readingsMap, const std::string& decodedSensorId) {
    if (!m_mqttClient || !m_mqttClient->connected()) return false;
    if (!isSafeIdentifier(decodedSensorId.c_str(), 64) || !readingsMap || readingsMap->empty() ||
        readingsMap->size() > ASCS_MAX_READINGS) return false;

    // Estimate capacity: Base + Map Size
    size_t mapSize = readingsMap ? readingsMap->size() : 0;
    size_t capacity = JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(mapSize) + (mapSize * 64) + 256;
    DynamicJsonDocument doc(capacity); // Use Dynamic for flexible sizing

    char nodeHex[9]; snprintf(nodeHex, sizeof(nodeHex), "%08lx", static_cast<unsigned long>(fromNode));
    doc["node_id"] = nodeHex;
    doc["sensor_id"] = decodedSensorId;
    doc["timestamp_utc"] = sensorData.timestamp_utc;
    doc["sequence_num"] = sensorData.sequence_num;

    JsonObject readings = doc.createNestedObject("readings");
    for (const auto& pair : *readingsMap) {
        if (!isSafeIdentifier(pair.first.c_str(), ASCS_MAX_READING_KEY_LENGTH) || !std::isfinite(pair.second)) return false;
        readings[pair.first] = pair.second;
    }

    char serviceId[11];
    snprintf(serviceId, sizeof(serviceId), "%lu", static_cast<unsigned long>(m_config.getServiceId()));
    std::string topic = m_config.getMqttBaseTopic() + "/sensor/" + serviceId + "/" + nodeHex;
    if (decodedSensorId.length() > 0) {
        topic += "/";
        topic += decodedSensorId;
    }

    std::string payload;
    serializeJson(doc, payload);
    if (doc.overflowed() || payload.size() + topic.size() + 16 > m_mqttClient->getSendBufferSize()) return false;
    return m_mqttClient->publish(topic.c_str(), payload.c_str(), false);
}

bool AkitaSmartCityServices::publishControlAckMqtt(const ControlAck &ack, uint32_t fromNode) {
    if (!m_mqttClient || !m_mqttClient->connected() || !isUuid(ack.command_id) ||
        ack.status < ControlAck_Status_REJECTED || ack.status > ControlAck_Status_FAILED) return false;
    StaticJsonDocument<320> doc;
    char nodeHex[9];
    snprintf(nodeHex, sizeof(nodeHex), "%08lx", static_cast<unsigned long>(fromNode));
    doc["commandId"] = ack.command_id;
    doc["nodeId"] = nodeHex;
    doc["status"] = ack.status == ControlAck_Status_EXECUTED ? "executed" :
                    ack.status == ControlAck_Status_FAILED ? "failed" : "rejected";
    doc["detail"] = ack.detail;

    std::string payload;
    serializeJson(doc, payload);
    const std::string topic = m_config.getMqttBaseTopic() + "/control/ack/" + ack.command_id;
    if (doc.overflowed() || payload.size() + topic.size() + 16 > m_mqttClient->getSendBufferSize()) return false;
    return m_mqttClient->publish(topic.c_str(), payload.c_str(), false);
}

void AkitaSmartCityServices::handleMqttCommand(const char *topic, const uint8_t *payload, size_t length) {
    if (!topic || !payload || length == 0 || length > 2048 || !m_initialized) return;
    const std::string prefix = m_config.getMqttBaseTopic() + "/control/";
    const std::string receivedTopic(topic);
    if (receivedTopic.compare(0, prefix.size(), prefix) != 0 || receivedTopic.find('/', prefix.size()) != std::string::npos) return;

    const std::string assetId = receivedTopic.substr(prefix.size());
    if (!isLowerHexNodeId(assetId)) return;
    char *end = nullptr;
    const unsigned long target = strtoul(assetId.c_str(), &end, 16);
    if (!end || *end != '\0' || target == 0 || target > UINT32_MAX) return;
    if (!isControlTarget(static_cast<uint32_t>(target))) return;

    DynamicJsonDocument doc(length + 512);
    if (deserializeJson(doc, payload, length) != DeserializationError::Ok || doc.overflowed() ||
        !doc.is<JsonObjectConst>() || doc.as<JsonObjectConst>().size() != 7 ||
        !doc["commandId"].is<const char *>() || !doc["assetId"].is<const char *>() ||
        !doc["action"].is<const char *>() || !doc["requestedAt"].is<const char *>() ||
        !doc["requestedBy"].is<const char *>() || !doc["expiresAtUtc"].is<unsigned long>()) return;
    const char *commandId = doc["commandId"].as<const char *>();
    const char *jsonAssetId = doc["assetId"].as<const char *>();
    const char *action = doc["action"].as<const char *>();
    const char *requestedAt = doc["requestedAt"].as<const char *>();
    const char *requestedBy = doc["requestedBy"].as<const char *>();
    const unsigned long expiresAtUtc = doc["expiresAtUtc"].as<unsigned long>();
    const uint32_t currentTime = getCurrentTime();
    if (!isUuid(commandId) || assetId != jsonAssetId || !isSafeAction(action) ||
        !isIsoUtcTimestamp(requestedAt) || !isValidRequester(requestedBy) || currentTime == 0 ||
        expiresAtUtc < currentTime || expiresAtUtc - currentTime > 120UL) return;

    const auto existing = m_pendingGatewayCommands.find(commandId);
    if (existing != m_pendingGatewayCommands.end() && existing->second.targetNode != target) return;
    if (existing == m_pendingGatewayCommands.end() && m_pendingGatewayCommands.size() >= ASCS_MAX_PENDING_COMMANDS) {
        ControlAck capacityAck = ControlAck_init_zero;
        snprintf(capacityAck.command_id, sizeof(capacityAck.command_id), "%s", commandId);
        capacityAck.status = ControlAck_Status_FAILED;
        snprintf(capacityAck.detail, sizeof(capacityAck.detail), "%s", "Gateway command queue is full");
        publishControlAckMqtt(capacityAck, static_cast<uint32_t>(target));
        return;
    }

    SmartCityPacket packet = SmartCityPacket_init_zero;
    packet.which_payload = SmartCityPacket_control_command_tag;
    packet.payload.control_command.target_node = static_cast<uint32_t>(target);
    snprintf(packet.payload.control_command.command_id, sizeof(packet.payload.control_command.command_id), "%s", commandId);
    snprintf(packet.payload.control_command.asset_id, sizeof(packet.payload.control_command.asset_id), "%s", jsonAssetId);
    snprintf(packet.payload.control_command.action, sizeof(packet.payload.control_command.action), "%s", action);
    packet.payload.control_command.expires_at_utc = static_cast<uint32_t>(expiresAtUtc);

    JsonVariantConst value = doc["value"];
    if (value.is<bool>()) {
        packet.payload.control_command.which_value = ControlCommand_bool_value_tag;
        packet.payload.control_command.value.bool_value = value.as<bool>();
    } else if (value.is<JsonFloat>() || value.is<JsonInteger>() || value.is<JsonUInt>()) {
        const float numeric = value.as<float>();
        if (!std::isfinite(numeric) || std::fabs(numeric) > ASCS_MAX_NUMERIC_COMMAND_VALUE) return;
        packet.payload.control_command.which_value = ControlCommand_numeric_value_tag;
        packet.payload.control_command.value.numeric_value = numeric;
    } else {
        return;
    }
    if (sendMessage(static_cast<uint32_t>(target), packet)) {
        if (existing == m_pendingGatewayCommands.end()) {
            ControlAck emptyAcknowledgement = ControlAck_init_zero;
            m_pendingGatewayCommands.emplace(commandId, PendingGatewayCommand{
                static_cast<uint32_t>(target), millis(), false, emptyAcknowledgement});
        }
    } else {
        ControlAck ack = ControlAck_init_zero;
        snprintf(ack.command_id, sizeof(ack.command_id), "%s", commandId);
        ack.status = ControlAck_Status_FAILED;
        snprintf(ack.detail, sizeof(ack.detail), "%s", "Gateway could not enqueue the mesh packet");
        publishControlAckMqtt(ack, static_cast<uint32_t>(target));
    }
}

void AkitaSmartCityServices::processPendingControlCommands() {
    const unsigned long now = millis();
    for (auto pending = m_pendingGatewayCommands.begin(); pending != m_pendingGatewayCommands.end();) {
        if (pending->second.acknowledged &&
            publishControlAckMqtt(pending->second.acknowledgement, pending->second.targetNode)) {
            pending = m_pendingGatewayCommands.erase(pending);
        } else if (now - pending->second.sentAt > ASCS_PENDING_COMMAND_TTL_MS) {
            ControlAck timeout = ControlAck_init_zero;
            snprintf(timeout.command_id, sizeof(timeout.command_id), "%s", pending->first.c_str());
            timeout.status = ControlAck_Status_FAILED;
            snprintf(timeout.detail, sizeof(timeout.detail), "%s", "Gateway timed out waiting for the target device");
            if (publishControlAckMqtt(timeout, pending->second.targetNode)) {
                pending = m_pendingGatewayCommands.erase(pending);
            } else {
                ++pending;
            }
        } else {
            ++pending;
        }
    }
}

void AkitaSmartCityServices::bufferPacket(const SensorData &sensorData, uint32_t fromNode, const std::map<std::string, float>* readingsMap, const std::string& decodedSensorId) {
    if (!m_fileSystemReady || !readingsMap) {
        LOG_ERROR("[%s] Telemetry was not buffered because persistent storage is unavailable.", getName());
        return;
    }

    SmartCityPacket packet = SmartCityPacket_init_zero;
    packet.which_payload = SmartCityPacket_sensor_data_tag;
    packet.payload.sensor_data = sensorData;

    snprintf(packet.payload.sensor_data.sensor_id, sizeof(packet.payload.sensor_data.sensor_id), "%s", decodedSensorId.c_str());

    MapCallbackContext encode_context;
    encode_context.map_ptr = const_cast<std::map<std::string, float>*>(readingsMap);
    packet.payload.sensor_data.readings.funcs.encode = encode_map_callback;
    packet.payload.sensor_data.readings.arg = &encode_context;

    uint8_t buffer[ASCS_GATEWAY_MAX_PACKET_SIZE];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    if (!pb_encode(&stream, SmartCityPacket_fields, &packet)) {
        LOG_ERROR("[%s] Buffer encode failed", getName());
        return;
    }
    const size_t pktLen = stream.bytes_written;
    const size_t recordSize = ASCS_GATEWAY_BUFFER_HEADER_SIZE + pktLen;
    if (recordSize > ASCS_GATEWAY_BUFFER_MAX_SIZE) {
        LOG_ERROR("[%s] Encoded telemetry record exceeds the persistent queue capacity.", getName());
        return;
    }

    File existing = FileSystem.open(ASCS_GATEWAY_BUFFER_FILENAME, FILE_READ);
    size_t existingSize = existing ? existing.size() : 0;
    if (existing) existing.close();
    while (existingSize + recordSize > ASCS_GATEWAY_BUFFER_MAX_SIZE) {
        if (!removePacketFromBuffer()) {
            LOG_ERROR("[%s] Persistent telemetry queue could not make room for a new record.", getName());
            return;
        }
        existing = FileSystem.open(ASCS_GATEWAY_BUFFER_FILENAME, FILE_READ);
        existingSize = existing ? existing.size() : 0;
        if (existing) existing.close();
    }

    File file = FileSystem.open(ASCS_GATEWAY_BUFFER_FILENAME, FILE_APPEND);
    if (!file) {
        LOG_ERROR("[%s] Persistent telemetry queue could not be opened.", getName());
        return;
    }

    const uint16_t len16 = static_cast<uint16_t>(pktLen);
    const uint32_t magic = ASCS_GATEWAY_BUFFER_MAGIC;
    const uint16_t version = ASCS_GATEWAY_BUFFER_VERSION;
    const uint32_t crc = bufferRecordCrc(fromNode, buffer, pktLen);
    const bool written = file.write(reinterpret_cast<const uint8_t*>(&magic), sizeof(magic)) == sizeof(magic) &&
                         file.write(reinterpret_cast<const uint8_t*>(&version), sizeof(version)) == sizeof(version) &&
                         file.write(reinterpret_cast<const uint8_t*>(&len16), sizeof(len16)) == sizeof(len16) &&
                         file.write(reinterpret_cast<const uint8_t*>(&fromNode), sizeof(fromNode)) == sizeof(fromNode) &&
                         file.write(reinterpret_cast<const uint8_t*>(&crc), sizeof(crc)) == sizeof(crc) &&
                         file.write(buffer, pktLen) == pktLen;
    file.flush();
    file.close();
    if (!written) {
        LOG_ERROR("[%s] Persistent telemetry queue write was incomplete.", getName());
        FileSystem.remove(ASCS_GATEWAY_BUFFER_FILENAME);
        m_gatewayBufferActive = false;
        return;
    }

    LOG_INFO("[%s] Buffered %u bytes from 0x%x", getName(), static_cast<unsigned>(pktLen), fromNode);
}

bool AkitaSmartCityServices::readPacketFromBuffer(fs::File &file, uint8_t* buffer, size_t &len, uint32_t &fromNode) {
    if (file.available() < ASCS_GATEWAY_BUFFER_HEADER_SIZE) return false;
    uint32_t magic;
    uint16_t version;
    uint16_t pktLen;
    uint32_t expectedCrc;
    if (file.read(reinterpret_cast<uint8_t*>(&magic), sizeof(magic)) != sizeof(magic) ||
        file.read(reinterpret_cast<uint8_t*>(&version), sizeof(version)) != sizeof(version) ||
        file.read(reinterpret_cast<uint8_t*>(&pktLen), sizeof(pktLen)) != sizeof(pktLen) ||
        file.read(reinterpret_cast<uint8_t*>(&fromNode), sizeof(fromNode)) != sizeof(fromNode) ||
        file.read(reinterpret_cast<uint8_t*>(&expectedCrc), sizeof(expectedCrc)) != sizeof(expectedCrc)) return false;
    if (magic != ASCS_GATEWAY_BUFFER_MAGIC || version != ASCS_GATEWAY_BUFFER_VERSION || fromNode == 0 ||
        pktLen == 0 || pktLen > ASCS_GATEWAY_MAX_PACKET_SIZE || file.available() < pktLen) return false;
    if (file.read(buffer, pktLen) != pktLen) return false;
    if (bufferRecordCrc(fromNode, buffer, pktLen) != expectedCrc) return false;
    len = pktLen;
    return true;
}

bool AkitaSmartCityServices::removePacketFromBuffer() {
    File r = FileSystem.open(ASCS_GATEWAY_BUFFER_FILENAME, FILE_READ);
    if (!r) return false;

    // Read the validated structural header to find the first record boundary.
    uint32_t magic;
    uint16_t version;
    uint32_t fromNode;
    uint16_t pktLen;
    uint32_t crc;
    if (r.read(reinterpret_cast<uint8_t*>(&magic), sizeof(magic)) != sizeof(magic) ||
        r.read(reinterpret_cast<uint8_t*>(&version), sizeof(version)) != sizeof(version) ||
        r.read(reinterpret_cast<uint8_t*>(&pktLen), sizeof(pktLen)) != sizeof(pktLen) ||
        r.read(reinterpret_cast<uint8_t*>(&fromNode), sizeof(fromNode)) != sizeof(fromNode) ||
        r.read(reinterpret_cast<uint8_t*>(&crc), sizeof(crc)) != sizeof(crc) ||
        magic != ASCS_GATEWAY_BUFFER_MAGIC || version != ASCS_GATEWAY_BUFFER_VERSION ||
        fromNode == 0 || pktLen == 0 || pktLen > ASCS_GATEWAY_MAX_PACKET_SIZE) {
        r.close();
        return FileSystem.remove(ASCS_GATEWAY_BUFFER_FILENAME);
    }

    size_t skip = ASCS_GATEWAY_BUFFER_HEADER_SIZE + pktLen;
    size_t total = r.size();

    if (skip >= total) {
        r.close();
        return FileSystem.remove(ASCS_GATEWAY_BUFFER_FILENAME);
    }

    FileSystem.remove(ASCS_GATEWAY_BUFFER_TEMP_FILENAME);
    File w = FileSystem.open(ASCS_GATEWAY_BUFFER_TEMP_FILENAME, FILE_WRITE);
    if (!w || !r.seek(skip)) {
        if (w) w.close();
        r.close();
        FileSystem.remove(ASCS_GATEWAY_BUFFER_TEMP_FILENAME);
        return false;
    }

    uint8_t buf[256];
    while(r.available()) {
        size_t n = r.read(buf, sizeof(buf));
        if (n == 0 || w.write(buf, n) != n) {
            r.close();
            w.close();
            FileSystem.remove(ASCS_GATEWAY_BUFFER_TEMP_FILENAME);
            return false;
        }
    }
    w.flush();
    r.close();
    w.close();
    if (!FileSystem.remove(ASCS_GATEWAY_BUFFER_FILENAME)) {
        FileSystem.remove(ASCS_GATEWAY_BUFFER_TEMP_FILENAME);
        return false;
    }
    if (!FileSystem.rename(ASCS_GATEWAY_BUFFER_TEMP_FILENAME, ASCS_GATEWAY_BUFFER_FILENAME)) {
        m_fileSystemReady = false;
        return false;
    }
    return true;
}

bool AkitaSmartCityServices::recoverBufferFile() {
    File queue = FileSystem.open(ASCS_GATEWAY_BUFFER_FILENAME, FILE_READ);
    const bool queueExists = static_cast<bool>(queue);
    if (queue) queue.close();
    File temporary = FileSystem.open(ASCS_GATEWAY_BUFFER_TEMP_FILENAME, FILE_READ);
    const bool temporaryExists = static_cast<bool>(temporary);
    if (temporary) temporary.close();
    if (!temporaryExists) return true;
    if (queueExists) return FileSystem.remove(ASCS_GATEWAY_BUFFER_TEMP_FILENAME);
    return FileSystem.rename(ASCS_GATEWAY_BUFFER_TEMP_FILENAME, ASCS_GATEWAY_BUFFER_FILENAME);
}

void AkitaSmartCityServices::processBufferedPackets() {
    if (!m_fileSystemReady) return;
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
            if (publishMqtt(scp.payload.sensor_data, fromNode, &decoded_readings, scp.payload.sensor_data.sensor_id)) {
                if (!removePacketFromBuffer()) LOG_ERROR("[%s] Published telemetry could not be removed from the queue.", getName());
                m_lastBufferProcessTime = millis(); // Reset timer to process next quickly
            }
        } else {
            // Corrupt, remove
            if (!removePacketFromBuffer()) LOG_ERROR("[%s] Corrupt telemetry record could not be removed.", getName());
        }
    } else {
        f.close();
        FileSystem.remove(ASCS_GATEWAY_BUFFER_FILENAME); // Corrupt file
    }
}
#endif

// --- Service and device state ---
void AkitaSmartCityServices::setSensor(std::unique_ptr<SensorInterface> sensor) {
    if (!m_initialized) m_sensor = std::move(sensor);
}
void AkitaSmartCityServices::setActuator(std::unique_ptr<ActuatorInterface> actuator) {
    if (!m_initialized) m_actuator = std::move(actuator);
}
ServiceDiscovery_Role AkitaSmartCityServices::getNodeRole() const { return m_config.getNodeRole(); }
void AkitaSmartCityServices::handleServiceDiscovery(const ServiceDiscovery &discovery, uint32_t fromNode) {
    if (fromNode == 0 || discovery.service_id == 0 ||
        discovery.node_role < ServiceDiscovery_Role_SENSOR || discovery.node_role > ServiceDiscovery_Role_GATEWAY) return;
    updateServiceTable(fromNode, discovery.node_role, discovery.service_id);
}
void AkitaSmartCityServices::updateServiceTable(uint32_t nodeId, ServiceDiscovery_Role role, uint32_t serviceId) {
    if (nodeId == getNodeNumber()) return;
    concurrency::LockGuard stateGuard(&m_stateLock);
    m_serviceTable[nodeId] = {role, serviceId, millis()};
}

uint32_t AkitaSmartCityServices::getNodeNumber() const {
    return nodeDB ? nodeDB->getNodeNum() : 0;
}

uint32_t AkitaSmartCityServices::getCurrentTime() const {
    return getValidTime(RTCQualityDevice);
}
void AkitaSmartCityServices::cleanupServiceTable() {
    unsigned long now = millis();
    concurrency::LockGuard stateGuard(&m_stateLock);
    for (auto it = m_serviceTable.begin(); it != m_serviceTable.end(); ) {
        if (now - it->second.lastSeen > m_config.getServiceTimeoutMs()) {
            it = m_serviceTable.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = m_recentCommands.begin(); it != m_recentCommands.end(); ) {
        if (now - it->second.completedAt > m_config.getServiceTimeoutMs()) it = m_recentCommands.erase(it);
        else ++it;
    }
}
uint32_t AkitaSmartCityServices::findGatewayNode() {
    concurrency::LockGuard stateGuard(&m_stateLock);
    for (const auto& entry : m_serviceTable) {
        if (entry.second.role == ServiceDiscovery_Role_GATEWAY && entry.second.serviceId == m_config.getServiceId()) {
            return entry.first;
        }
    }
    return 0; // No gateway found
}
bool AkitaSmartCityServices::isControlTarget(uint32_t nodeId) {
    concurrency::LockGuard stateGuard(&m_stateLock);
    const auto target = m_serviceTable.find(nodeId);
    return target != m_serviceTable.end() && target->second.role == ServiceDiscovery_Role_SENSOR &&
           target->second.serviceId == m_config.getServiceId();
}
bool AkitaSmartCityServices::isTelemetryRouteAllowed(uint32_t originNode, uint32_t fromNode) {
    concurrency::LockGuard stateGuard(&m_stateLock);
    const auto source = m_serviceTable.find(originNode);
    if (source == m_serviceTable.end() || source->second.role != ServiceDiscovery_Role_SENSOR ||
        source->second.serviceId != m_config.getServiceId()) return false;
    if (m_config.getNodeRole() == ServiceDiscovery_Role_AGGREGATOR) return originNode == fromNode;
    if (m_config.getNodeRole() != ServiceDiscovery_Role_GATEWAY) return false;
    if (originNode == fromNode) return true;
    const auto relay = m_serviceTable.find(fromNode);
    return relay != m_serviceTable.end() && relay->second.role == ServiceDiscovery_Role_AGGREGATOR &&
           relay->second.serviceId == m_config.getServiceId();
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
        LOG_INFO("[%s] Connecting to WiFi SSID='%s'...\n", getName(), ssid.c_str());
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
        LOG_INFO("[%s] WiFi disconnected, attempting reconnect...\n", getName());
        WiFi.disconnect();
        WiFi.begin(m_config.getWifiSsid().c_str(), m_config.getWifiPassword().c_str());
    }
}

void AkitaSmartCityServices::connectMQTT() {
    if (!m_mqttClient) return;
    if (m_mqttClient->connected()) return;

    if (WiFi.status() != WL_CONNECTED) {
        LOG_DEBUG("[ASCS] Skipping MQTT connect: WiFi not connected");
        return;
    }

    char clientId[48];
    uint32_t svc = m_config.getServiceId();
    snprintf(clientId, sizeof(clientId), "ascs_gw_%08lx_%lu", static_cast<unsigned long>(getNodeNumber()),
             static_cast<unsigned long>(svc));

    LOG_INFO("[%s] Connecting to MQTT broker %s:%d as %s\n", getName(), m_config.getMqttServer().c_str(), m_config.getMqttPort(), clientId);

    const std::string user = m_config.getMqttUser();
    const std::string pass = m_config.getMqttPassword();
    const bool ok = m_mqttClient->connect(clientId, user.c_str(), pass.c_str());

    if (ok) {
        LOG_INFO("[%s] MQTT connected.\n", getName());
        const std::string commandTopic = m_config.getMqttBaseTopic() + "/control/+";
        if (!m_mqttClient->subscribe(commandTopic.c_str(), 1)) {
            LOG_ERROR("[%s] MQTT command subscription failed.\n", getName());
            m_mqttClient->disconnect();
        }
    } else {
        LOG_WARN("[%s] MQTT connect failed\n", getName());
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
    if (!AkitaSmartCityServices::s_instance) return;
    AkitaSmartCityServices::s_instance->handleMqttCommand(topic, payload, length);
}
#endif
