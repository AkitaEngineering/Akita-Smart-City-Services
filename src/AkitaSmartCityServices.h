#ifndef AKITASMARTCITYSERVICES_H
#define AKITASMARTCITYSERVICES_H

#include "meshtastic.h"      // Main Meshtastic library
#include "mesh_portnums.h"   // For PortNum enum definition
#include "plugin_api.h"      // Required for Meshtastic plugins
#include "SmartCity.pb.h"    // Generated header from SmartCity.proto
#include "SensorInterface.h" // Abstract sensor interface

// Standard C++/System Libraries
#include <vector>
#include <map>
#include <string>
#include <memory> // For std::unique_ptr

// Forward declarations for libraries used only in .cpp
class PubSubClient;
class WiFiClient;
class Preferences;

// --- Configuration Constants ---

// Define the Meshtastic PortNum used for ASCS communication.
// Using a value from the custom application range.
#define ASCS_PORT_NUM (PortNum)(PortNum_APP_CUSTOM_MIN + 1)

// Default broadcast address for Meshtastic
#define ASCS_BROADCAST_ADDR BROADCAST_ADDR // Use Meshtastic's definition

// Default configuration values (used if not found in Preferences)
#define ASCS_DEFAULT_ROLE ServiceDiscovery_Role_SENSOR // Default to Sensor role
#define ASCS_DEFAULT_SERVICE_ID 1 // Default service group ID
#define ASCS_DEFAULT_TARGET_NODE 0 // Default target node ID (0 means broadcast/discover)
#define ASCS_DEFAULT_SENSOR_READ_INTERVAL_MS 60000 // 1 minute
#define ASCS_DEFAULT_DISCOVERY_INTERVAL_MS 300000 // 5 minutes
#define ASCS_DEFAULT_SERVICE_TIMEOUT_MS 900000 // 15 minutes (3x discovery interval)
#define ASCS_DEFAULT_MQTT_RECONNECT_INTERVAL_MS 10000 // 10 seconds

// Default WiFi/MQTT credentials (CHANGE THESE IN YOUR CONFIGURATION!)
#define ASCS_DEFAULT_WIFI_SSID "YourWiFi_SSID"
#define ASCS_DEFAULT_WIFI_PASSWORD "YourWiFiPassword"
#define ASCS_DEFAULT_MQTT_SERVER "your_mqtt_broker.com"
#define ASCS_DEFAULT_MQTT_PORT 1883
#define ASCS_DEFAULT_MQTT_USER "" // Empty if no auth
#define ASCS_DEFAULT_MQTT_PASSWORD "" // Empty if no auth
#define ASCS_DEFAULT_MQTT_BASE_TOPIC "akita/smartcity" // Base topic for MQTT messages

// --- Main Plugin Class ---

class AkitaSmartCityServices : public MeshtasticPlugin {
public:
    /**
     * @brief Constructor.
     * @param name The name of the plugin instance.
     */
    AkitaSmartCityServices(const char *name = "ASCS");

    /**
     * @brief Destructor.
     */
    virtual ~AkitaSmartCityServices();

    // --- Meshtastic Plugin API Methods ---

    /**
     * @brief Initialize the plugin. Called once by Meshtastic during startup.
     * @param api Pointer to the Meshtastic API.
     */
    virtual void init(const MeshtasticAPI *api) override;

    /**
     * @brief Main update loop for the plugin. Called repeatedly by Meshtastic.
     */
    virtual void loop() override;

    /**
     * @brief Handles received Meshtastic packets.
     * @param packet The received mesh packet.
     * @return True if the packet was handled by this plugin, false otherwise.
     */
    virtual bool handleReceived(const meshPacket &packet) override;

    // --- Public Configuration Methods ---

    /**
     * @brief Sets the sensor implementation to be used by this node.
     * Should be called *before* init() or begin().
     * Takes ownership of the sensor object.
     * @param sensor Pointer to a SensorInterface implementation.
     */
    void setSensor(std::unique_ptr<SensorInterface> sensor);

    // --- Public Information Methods ---

    /**
     * @brief Gets the configured role of this node.
     * @return The ServiceDiscovery_Role enum value.
     */
    ServiceDiscovery_Role getNodeRole() const;

private:
    // --- Internal Helper Methods ---

    // Configuration Management
    void loadConfig();
    void saveConfig(); // Optional: If dynamic config changes are needed

    // Network Management (Gateway Role)
    void connectWiFi();
    void checkWiFiConnection();
    void connectMQTT();
    void checkMQTTConnection();
    static void mqttCallback(char *topic, byte *payload, unsigned int length); // Static for PubSubClient

    // Packet Handling
    void handleServiceDiscovery(const ServiceDiscovery &discovery, uint32_t fromNode);
    void handleSensorData(const SensorData &sensorData, uint32_t fromNode);

    // Message Sending
    void sendServiceDiscovery(uint32_t toNode = ASCS_BROADCAST_ADDR);
    void sendSensorData(const SensorData &sensorData);
    bool sendMessage(uint32_t toNode, const SmartCityPacket &packet);

    // Role-Specific Logic
    void runSensorLogic();
    void runAggregatorLogic(const SensorData &sensorData, uint32_t fromNode);
    void runGatewayLogic(const SensorData &sensorData, uint32_t fromNode);

    // Service Discovery Management
    void updateServiceTable(uint32_t nodeId, ServiceDiscovery_Role role, uint32_t serviceId);
    void cleanupServiceTable();
    uint32_t findGatewayNode(); // Finds a suitable gateway from the service table

    // MQTT Publishing (Gateway Role)
    void publishMqtt(const SensorData &sensorData, uint32_t fromNode);

    // --- Member Variables ---

    const MeshtasticAPI *m_api = nullptr; // Pointer to Meshtastic API

    // Configuration Parameters (loaded from Preferences)
    ServiceDiscovery_Role m_nodeRole = ASCS_DEFAULT_ROLE;
    uint32_t m_serviceId = ASCS_DEFAULT_SERVICE_ID;
    uint32_t m_targetNodeId = ASCS_DEFAULT_TARGET_NODE; // Preferred Gateway/Aggregator ID
    uint32_t m_sensorReadIntervalMs = ASCS_DEFAULT_SENSOR_READ_INTERVAL_MS;
    uint32_t m_discoveryIntervalMs = ASCS_DEFAULT_DISCOVERY_INTERVAL_MS;
    uint32_t m_serviceTimeoutMs = ASCS_DEFAULT_SERVICE_TIMEOUT_MS;

    // WiFi/MQTT Config (Gateway only)
    std::string m_wifiSsid;
    std::string m_wifiPassword;
    std::string m_mqttServer;
    int m_mqttPort = ASCS_DEFAULT_MQTT_PORT;
    std::string m_mqttUser;
    std::string m_mqttPassword;
    std::string m_mqttBaseTopic;

    // Timers
    unsigned long m_lastSensorReadTime = 0;
    unsigned long m_lastDiscoverySendTime = 0;
    unsigned long m_lastServiceCleanupTime = 0;
    unsigned long m_lastMqttReconnectAttempt = 0;

    // State Variables
    uint32_t m_sensorSequenceNum = 0; // Sequence number for sensor data

    // Sensor Implementation
    std::unique_ptr<SensorInterface> m_sensor = nullptr;

    // Service Discovery Table
    struct DiscoveredService {
        ServiceDiscovery_Role role;
        uint32_t serviceId;
        unsigned long lastSeen;
    };
    std::map<uint32_t, DiscoveredService> m_serviceTable; // Key: Node ID

    // Network Clients (Gateway Role) - Use pointers to avoid global instances
    WiFiClient *m_wifiClient = nullptr;
    PubSubClient *m_mqttClient = nullptr;
    Preferences *m_preferences = nullptr; // Preferences handler

    // Static instance pointer for MQTT callback
    static AkitaSmartCityServices* s_instance;
};

#endif // AKITASMARTCITYSERVICES_H
