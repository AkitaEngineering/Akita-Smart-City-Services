#ifndef AKITASMARTCITYSERVICES_H
#define AKITASMARTCITYSERVICES_H

#include "meshtastic.h"      // Main Meshtastic library
#include "mesh_portnums.h"   // For PortNum enum definition
#include "plugin_api.h"      // Required for Meshtastic plugins
// Make sure the path to the generated proto header is correct for your build system
#include "generated_proto/SmartCity.pb.h" // Generated header from SmartCity.proto
#include "interfaces/SensorInterface.h" // Abstract sensor interface
#include "ASCSConfig.h"      // Include the new config manager header

// Standard C++/System Libraries
#include <vector>
#include <map>
#include <string>
#include <memory> // For std::unique_ptr

// Forward declarations for libraries used only in .cpp
class PubSubClient;
class WiFiClient;
class File; // For SPIFFS/LittleFS

// --- Constants ---

// Define the Meshtastic PortNum used for ASCS communication.
// Using a value from the custom application range.
#define ASCS_PORT_NUM (PortNum)(PortNum_APP_CUSTOM_MIN + 1)

// Default broadcast address for Meshtastic
#define ASCS_BROADCAST_ADDR BROADCAST_ADDR // Use Meshtastic's definition

// Gateway Buffering Config
#define ASCS_GATEWAY_BUFFER_FILENAME "/ascs_buffer.dat" // Filename on the filesystem
#define ASCS_GATEWAY_BUFFER_MAX_SIZE (10 * 1024) // Max buffer file size (e.g., 10KB) - adjust as needed!
#define ASCS_GATEWAY_MAX_PACKET_SIZE 256 // Max size of a single encoded packet to buffer (should match SmartCityPacket_size or be slightly larger)

// --- Nanopb Map Callback Struct ---
// Structure to pass context (the map) to nanopb callbacks
// This is needed for both encoding and decoding map fields.
struct MapCallbackContext {
    // Pointer to the map being processed (either for encoding from or decoding into)
    std::map<std::string, float>* map_ptr = nullptr;
    // For encoding, we need an iterator to track progress
    std::map<std::string, float>::iterator map_iterator;
    // Flag to track success during encoding iteration (helps stop early on error)
    bool encode_successful = true;
};


// --- Main Plugin Class ---

class AkitaSmartCityServices : public MeshtasticPlugin {
public:
    /**
     * @brief Constructor.
     * @param name The name of the plugin instance (optional).
     */
    AkitaSmartCityServices(const char *name = "ASCS");

    /**
     * @brief Destructor. Cleans up allocated resources.
     */
    virtual ~AkitaSmartCityServices();

    // --- Meshtastic Plugin API Methods ---

    /**
     * @brief Initialize the plugin. Called once by Meshtastic during startup.
     * Loads configuration, initializes roles, sets up network clients if needed.
     * @param api Pointer to the Meshtastic API object.
     */
    virtual void init(const MeshtasticAPI *api) override;

    /**
     * @brief Main update loop for the plugin. Called repeatedly by Meshtastic.
     * Handles periodic tasks like sensor reads, discovery broadcasts, buffer processing.
     */
    virtual void loop() override;

    /**
     * @brief Handles received Meshtastic packets.
     * Checks if the packet is for ASCS, decodes it, and routes it to appropriate handlers.
     * @param packet The received mesh packet.
     * @return True if the packet was handled by this plugin, false otherwise.
     */
    virtual bool handleReceived(const meshPacket &packet) override;

    // --- Public Configuration Methods ---

    /**
     * @brief Sets the sensor implementation to be used by this node if configured as a Sensor.
     * Should be called *before* Meshtastic::begin() which calls plugin::init().
     * Takes ownership of the sensor object via std::unique_ptr.
     * @param sensor A unique_ptr to a SensorInterface implementation.
     */
    void setSensor(std::unique_ptr<SensorInterface> sensor);

    // --- Public Information Methods ---

    /**
     * @brief Gets the configured role of this node.
     * @return The ServiceDiscovery_Role enum value from the configuration.
     */
    ServiceDiscovery_Role getNodeRole() const;

    // --- Nanopb Map Field Callbacks ---
    // These functions implement the logic for encoding/decoding the map<string, float> field.
    // They must be static or global C-style functions to be used by nanopb.
    // The 'arg' parameter is used to pass context (like the map pointer) via MapCallbackContext.

    /**
     * @brief Nanopb callback function to encode the map<string, float> 'readings' field.
     * Iterates through the map provided in the context ('arg') and encodes each key-value pair
     * as a submessage stream.
     * @param stream The nanopb output stream.
     * @param field The field descriptor for the map field.
     * @param arg Pointer to a pointer to the MapCallbackContext structure.
     * @return True on success, false on failure.
     */
    static bool encode_map_callback(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);

    /**
     * @brief Nanopb callback function to decode the map<string, float> 'readings' field.
     * Decodes each key-value pair submessage from the stream and inserts it into the map
     * provided in the context ('arg').
     * @param stream The nanopb input stream.
     * @param field The field descriptor for the map field.
     * @param arg Pointer to a pointer to the MapCallbackContext structure.
     * @return True on success, false on failure.
     */
    static bool decode_map_callback(pb_istream_t *stream, const pb_field_t *field, void **arg);


private:
    // --- Internal Helper Methods ---

    // Network Management (Gateway Role)
    void connectWiFi();
    void checkWiFiConnection();
    void connectMQTT();
    void checkMQTTConnection();
    // Static callback required by PubSubClient library signature.
    static void mqttCallback(char *topic, byte *payload, unsigned int length);

    // Packet Handling
    void handleServiceDiscovery(const ServiceDiscovery &discovery, uint32_t fromNode);
    // Takes the decoded SensorData and the originating node ID.
    void handleSensorData(const SensorData &sensorData, uint32_t fromNode);

    // Message Sending
    void sendServiceDiscovery(uint32_t toNode = ASCS_BROADCAST_ADDR);
    // Takes a fully prepared SensorData struct (including map callbacks set if needed).
    void sendSensorData(const SensorData &sensorData);
    // Core function to encode and send any SmartCityPacket via Meshtastic.
    bool sendMessage(uint32_t toNode, const SmartCityPacket &packet);

    // Role-Specific Logic - Called from loop() or handleReceived()
    void runSensorLogic();
    // Aggregator logic now takes the full packet for potential forwarding.
    void runAggregatorLogic(const SmartCityPacket &packet, uint32_t fromNode);
    // Gateway logic now takes the full packet for potential buffering/publishing.
    void runGatewayLogic(const SmartCityPacket &packet, uint32_t fromNode);

    // Service Discovery Management
    void updateServiceTable(uint32_t nodeId, ServiceDiscovery_Role role, uint32_t serviceId);
    void cleanupServiceTable();
    uint32_t findGatewayNode(); // Finds a suitable gateway from the service table

    // MQTT Publishing & Buffering (Gateway Role)
    // Decides whether to publish directly or buffer based on MQTT connection status.
    void publishMqttOrBuffer(const SmartCityPacket &packet, uint32_t fromNode);
    // Performs the actual MQTT publication. Returns true on success.
    bool publishMqtt(const SensorData &sensorData, uint32_t fromNode);
    // Appends an encoded packet to the buffer file.
    void bufferPacket(const SmartCityPacket &packet);
    // Reads and sends packets stored in the buffer file.
    void processBufferedPackets();
    // Helper to read the next length-prefixed packet from the buffer file.
    bool readPacketFromBuffer(File &file, uint8_t* buffer, size_t &len);
    // Helper to remove the first packet from the buffer file (basic file copy method).
    void removePacketFromBuffer();

    // --- Member Variables ---

    const MeshtasticAPI *m_api = nullptr; // Pointer to Meshtastic API provided during init
    ASCSConfig m_config; // Configuration manager instance

    // Timers for periodic actions
    unsigned long m_lastSensorReadTime = 0;
    unsigned long m_lastDiscoverySendTime = 0;
    unsigned long m_lastServiceCleanupTime = 0;
    unsigned long m_lastMqttReconnectAttempt = 0;
    unsigned long m_lastBufferProcessTime = 0; // Timer for processing buffered messages

    // State Variables
    uint32_t m_sensorSequenceNum = 0; // Sequence number for sensor data packets
    bool m_gatewayBufferActive = false; // Flag indicating if buffering is currently needed/active

    // Sensor Implementation (if configured as Sensor role)
    std::unique_ptr<SensorInterface> m_sensor = nullptr;

    // Service Discovery Table - Maps Node ID to discovered service info
    struct DiscoveredService {
        ServiceDiscovery_Role role;
        uint32_t serviceId;
        unsigned long lastSeen; // Timestamp of last message/discovery
    };
    std::map<uint32_t, DiscoveredService> m_serviceTable;

    // Network Clients (Gateway Role) - Pointers to avoid global instances
    WiFiClient *m_wifiClient = nullptr;
    PubSubClient *m_mqttClient = nullptr;

    // Static instance pointer for MQTT callback context
    static AkitaSmartCityServices* s_instance;
};

#endif // AKITASMARTCITYSERVICES_H

