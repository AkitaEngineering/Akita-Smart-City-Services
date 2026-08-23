#ifndef AKITASMARTCITYSERVICES_H
#define AKITASMARTCITYSERVICES_H

#ifndef ASCS_OFFICIAL_FIRMWARE
#error "ASCS must be built as a module in the official Meshtastic firmware tree"
#endif

#include "SinglePortModule.h"
#include "concurrency/OSThread.h"
#include "concurrency/Lock.h"
#include "generated_proto/SmartCity.pb.h" // Generated header from SmartCity.proto
#include "interfaces/SensorInterface.h" // Abstract sensor interface
#include "interfaces/ActuatorInterface.h"
#include "ASCSConfig.h"      // Include the new config manager header
#include <Arduino.h>
#ifdef ASCS_ROLE_GATEWAY
#include <FS.h>
#endif

// Standard C++/System Libraries
#include <vector>
#include <map>
#include <string>
#include <memory> // For std::unique_ptr

// Forward declarations
class PubSubClient;
class WiFiClientSecure;

// --- Constants ---

#define ASCS_PORT_NUM meshtastic_PortNum_PRIVATE_APP
#define ASCS_BROADCAST_ADDR NODENUM_BROADCAST

// Gateway Buffering Config
#define ASCS_GATEWAY_BUFFER_FILENAME "/ascs_buffer.dat"
#define ASCS_GATEWAY_BUFFER_TEMP_FILENAME "/ascs_buffer.tmp"
#define ASCS_GATEWAY_BUFFER_MAX_SIZE (20 * 1024) // Increased to 20KB
#define ASCS_GATEWAY_MAX_PACKET_SIZE 512 // Increased max packet size for maps
#define ASCS_GATEWAY_BUFFER_MAGIC 0x41534353UL
#define ASCS_GATEWAY_BUFFER_VERSION 1U
#define ASCS_GATEWAY_BUFFER_HEADER_SIZE 16U
#define ASCS_MAX_READINGS 64
#define ASCS_MAX_READING_KEY_LENGTH 64
#define ASCS_MQTT_BUFFER_SIZE 1024
#define ASCS_MAX_PENDING_COMMANDS 32
#define ASCS_PENDING_COMMAND_TTL_MS (75UL * 1000UL)
#define ASCS_MAX_NUMERIC_COMMAND_VALUE 1000000000.0f

// --- Nanopb Map Callback Struct ---
struct MapCallbackContext {
    std::map<std::string, float>* map_ptr = nullptr;
    size_t decoded_entries = 0;
};

// --- Main Plugin Class ---

class AkitaSmartCityServices : public SinglePortModule, private concurrency::OSThread {
public:
    AkitaSmartCityServices(const char *name = "ASCS");
    virtual ~AkitaSmartCityServices();

    // --- Meshtastic Plugin API Methods ---
  protected:
    void setup() override;
    int32_t runOnce() override;
    ProcessMessage handleReceived(const meshtastic_MeshPacket &packet) override;
  public:

    // --- Public Configuration ---
    void setSensor(std::unique_ptr<SensorInterface> sensor);
    void setActuator(std::unique_ptr<ActuatorInterface> actuator);
    ServiceDiscovery_Role getNodeRole() const;
    const char *getName() const { return "ASCS"; }

    // --- Nanopb Map Field Callbacks ---
    static bool encode_map_callback(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);
    static bool decode_map_callback(pb_istream_t *stream, const pb_field_t *field, void **arg);

private:
    void initialize();
    void loop();
    bool handlePayload(const uint8_t *payload, size_t payloadLength, uint32_t fromNode);
    uint32_t getNodeNumber() const;
    uint32_t getCurrentTime() const;
    // --- Internal Helper Methods ---

    // Network Management (Gateway)
#ifdef ASCS_ROLE_GATEWAY
    void connectWiFi();
    void checkWiFiConnection();
    void connectMQTT();
    void checkMQTTConnection();
    static void mqttCallback(char *topic, byte *payload, unsigned int length);
#endif

    // Packet Handling
    void handleServiceDiscovery(const ServiceDiscovery &discovery, uint32_t fromNode);
    // Updated: Accepts optional map pointer and decoded sensor ID
    void handleSensorData(const SensorData &sensorData, uint32_t fromNode, const std::map<std::string, float>* readingsMap, const std::string& decodedSensorId = "");
    void handleControlCommand(const ControlCommand &command, uint32_t fromNode);
    void handleControlAck(const ControlAck &ack, uint32_t fromNode);

    // Message Sending
    void sendServiceDiscovery(uint32_t toNode = ASCS_BROADCAST_ADDR);
    void sendSensorData(const SensorData &sensorData, std::map<std::string, float>& readingsMap, const std::string &sensorId = std::string());
    bool sendMessage(uint32_t toNode, const SmartCityPacket &packet);

    // Role-Specific Logic
    void runSensorLogic();
    void runAggregatorLogic(const SmartCityPacket &packet, uint32_t fromNode);
    // Updated: Accepts optional map pointer and decoded sensor ID
    void runGatewayLogic(const SensorData &sensorData, uint32_t fromNode, const std::map<std::string, float>* readingsMap, const std::string& decodedSensorId);

    // Service Discovery
    void updateServiceTable(uint32_t nodeId, ServiceDiscovery_Role role, uint32_t serviceId);
    void cleanupServiceTable();
    uint32_t findGatewayNode();
    bool isControlTarget(uint32_t nodeId);
    bool isTelemetryRouteAllowed(uint32_t originNode, uint32_t fromNode);

    // Gateway Buffering & MQTT
    // Updated: Accepts map for JSON generation/re-encoding and decoded sensor ID
    void publishMqttOrBuffer(const SensorData &sensorData, uint32_t fromNode, const std::map<std::string, float>* readingsMap, const std::string& decodedSensorId);
    bool publishMqtt(const SensorData &sensorData, uint32_t fromNode, const std::map<std::string, float>* readingsMap, const std::string& decodedSensorId);
    bool publishControlAckMqtt(const ControlAck &ack, uint32_t fromNode);
    void handleMqttCommand(const char *topic, const uint8_t *payload, size_t length);
    void processPendingControlCommands();
    void sendControlAck(uint32_t toNode, const char *commandId, ControlAck_Status status, const std::string &detail);
    void bufferPacket(const SensorData &sensorData, uint32_t fromNode, const std::map<std::string, float>* readingsMap, const std::string& decodedSensorId);

    void processBufferedPackets();
    bool recoverBufferFile();
    // Updated: Reads fromNode from buffer
#ifdef ASCS_ROLE_GATEWAY
    bool readPacketFromBuffer(fs::File &file, uint8_t* buffer, size_t &len, uint32_t &fromNode);
#endif
    bool removePacketFromBuffer();

    // --- Member Variables ---
    bool m_initialized = false;
    ASCSConfig m_config;

    unsigned long m_lastSensorReadTime = 0;
    unsigned long m_lastDiscoverySendTime = 0;
    unsigned long m_lastServiceCleanupTime = 0;
    unsigned long m_lastMqttReconnectAttempt = 0;
    unsigned long m_lastBufferProcessTime = 0;

    uint32_t m_sensorSequenceNum = 0;
    bool m_gatewayBufferActive = false;
    bool m_fileSystemReady = false;

    std::unique_ptr<SensorInterface> m_sensor = nullptr;
    std::unique_ptr<ActuatorInterface> m_actuator = nullptr;

    struct DiscoveredService {
        ServiceDiscovery_Role role;
        uint32_t serviceId;
        unsigned long lastSeen;
    };
    std::map<uint32_t, DiscoveredService> m_serviceTable;

    struct CachedControlResult {
        ControlAck_Status status;
        std::string detail;
        unsigned long completedAt;
    };
    std::map<std::string, CachedControlResult> m_recentCommands;
    struct PendingGatewayCommand {
        uint32_t targetNode;
        unsigned long sentAt;
        bool acknowledged;
        ControlAck acknowledgement;
    };
    std::map<std::string, PendingGatewayCommand> m_pendingGatewayCommands;
    concurrency::Lock m_stateLock;
    concurrency::Lock m_gatewayLock;

    WiFiClientSecure *m_wifiClient = nullptr;
    PubSubClient *m_mqttClient = nullptr;

    static AkitaSmartCityServices* s_instance;
};

#endif // AKITASMARTCITYSERVICES_H
