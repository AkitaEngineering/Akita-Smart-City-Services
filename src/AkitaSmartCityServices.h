#ifndef AKITASMARTCITYSERVICES_H
#define AKITASMARTCITYSERVICES_H

#include "meshtastic.h"      // Main Meshtastic library
#include "mesh_portnums.h"   // For PortNum enum definition
#include "plugin_api.h"      // Required for Meshtastic plugins
#include "generated_proto/SmartCity.pb.h" // Generated header from SmartCity.proto
#include "interfaces/SensorInterface.h" // Abstract sensor interface
#include "ASCSConfig.h"      // Include the new config manager header
#include <Arduino.h>

// Standard C++/System Libraries
#include <vector>
#include <map>
#include <string>
#include <memory> // For std::unique_ptr

// Forward declarations
class PubSubClient;
class WiFiClient;
class File; 

// --- Constants ---

#define ASCS_PORT_NUM (PortNum)(PortNum_APP_CUSTOM_MIN + 1)
#define ASCS_BROADCAST_ADDR BROADCAST_ADDR

// Gateway Buffering Config
#define ASCS_GATEWAY_BUFFER_FILENAME "/ascs_buffer.dat"
#define ASCS_GATEWAY_BUFFER_MAX_SIZE (20 * 1024) // Increased to 20KB
#define ASCS_GATEWAY_MAX_PACKET_SIZE 512 // Increased max packet size for maps

// --- Nanopb Map Callback Struct ---
struct MapCallbackContext {
    std::map<std::string, float>* map_ptr = nullptr;
    std::map<std::string, float>::const_iterator map_iterator; // Changed to const_iterator
    bool encode_successful = true;
};

// --- Main Plugin Class ---

class AkitaSmartCityServices : public MeshtasticPlugin {
public:
    AkitaSmartCityServices(const char *name = "ASCS");
    virtual ~AkitaSmartCityServices();

    // --- Meshtastic Plugin API Methods ---
    virtual void init(const MeshtasticAPI *api) override;
    virtual void loop() override;
    virtual bool handleReceived(const meshPacket *packet) override;

    // --- Public Configuration ---
    void setSensor(std::unique_ptr<SensorInterface> sensor);
    ServiceDiscovery_Role getNodeRole() const;

    // --- Nanopb Map Field Callbacks ---
    static bool encode_map_callback(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);
    static bool decode_map_callback(pb_istream_t *stream, const pb_field_t *field, void **arg);

private:
    // --- Internal Helper Methods ---

    // Network Management (Gateway)
    void connectWiFi();
    void checkWiFiConnection();
    void connectMQTT();
    void checkMQTTConnection();
    static void mqttCallback(char *topic, byte *payload, unsigned int length);

    // Packet Handling
    void handleServiceDiscovery(const ServiceDiscovery &discovery, uint32_t fromNode);
    // Updated: Accepts optional map pointer
    void handleSensorData(const SensorData &sensorData, uint32_t fromNode, const std::map<std::string, float>* readingsMap);

    // Message Sending
    void sendServiceDiscovery(uint32_t toNode = ASCS_BROADCAST_ADDR);
    void sendSensorData(const SensorData &sensorData, std::map<std::string, float>& readingsMap, const std::string &sensorId = std::string()); 
    bool sendMessage(uint32_t toNode, const SmartCityPacket &packet);

    // Role-Specific Logic
    void runSensorLogic();
    void runAggregatorLogic(const SmartCityPacket &packet, uint32_t fromNode);
    // Updated: Accepts optional map pointer
    void runGatewayLogic(const SensorData &sensorData, uint32_t fromNode, const std::map<std::string, float>* readingsMap);

    // Service Discovery
    void updateServiceTable(uint32_t nodeId, ServiceDiscovery_Role role, uint32_t serviceId);
    void cleanupServiceTable();
    uint32_t findGatewayNode();

    // Gateway Buffering & MQTT
    // Updated: Accepts map for JSON generation/re-encoding
    void publishMqttOrBuffer(const SensorData &sensorData, uint32_t fromNode, const std::map<std::string, float>* readingsMap);
    bool publishMqtt(const SensorData &sensorData, uint32_t fromNode, const std::map<std::string, float>* readingsMap);
    void bufferPacket(const SensorData &sensorData, uint32_t fromNode, const std::map<std::string, float>* readingsMap);
    
    void processBufferedPackets();
    // Updated: Reads fromNode from buffer
    bool readPacketFromBuffer(File &file, uint8_t* buffer, size_t &len, uint32_t &fromNode);
    void removePacketFromBuffer();

    // --- Member Variables ---
    const MeshtasticAPI *m_api = nullptr;
    ASCSConfig m_config;

    unsigned long m_lastSensorReadTime = 0;
    unsigned long m_lastDiscoverySendTime = 0;
    unsigned long m_lastServiceCleanupTime = 0;
    unsigned long m_lastMqttReconnectAttempt = 0;
    unsigned long m_lastBufferProcessTime = 0;

    uint32_t m_sensorSequenceNum = 0;
    bool m_gatewayBufferActive = false;

    std::unique_ptr<SensorInterface> m_sensor = nullptr;

    struct DiscoveredService {
        ServiceDiscovery_Role role;
        uint32_t serviceId;
        unsigned long lastSeen;
    };
    std::map<uint32_t, DiscoveredService> m_serviceTable;

    WiFiClient *m_wifiClient = nullptr;
    PubSubClient *m_mqttClient = nullptr;

    static AkitaSmartCityServices* s_instance;
};

#endif // AKITASMARTCITYSERVICES_H

