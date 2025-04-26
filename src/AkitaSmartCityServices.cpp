#include "AkitaSmartCityServices.h" // Corresponding header
#include "meshtastic.h"             // Main Meshtastic API
#include "plugin_api.h"             // Plugin API definitions
#include "pb_encode.h"              // Nanopb encoding functions
#include "pb_decode.h"              // Nanopb decoding functions
#include "mesh_packet.h"            // For meshPacket definition
#include "globals.h"                // Access to global objects like radio, NodeDB

// Required Libraries (conditional includes for Gateway role)
#ifdef ASCS_ROLE_GATEWAY
#include <WiFi.h>          // For WiFi connectivity
#include <PubSubClient.h>  // For MQTT client
#include <ArduinoJson.h>   // For formatting MQTT payload as JSON

// Choose ONE filesystem library:
#include <SPIFFS.h>        // Option 1: Default ESP32 filesystem
// #include <LittleFS.h>   // Option 2: Often preferred on ESP32 for wear leveling
#define FileSystem SPIFFS  // Define which filesystem to use (SPIFFS or LittleFS)
#endif

// Nanopb includes
#include "pb_common.h"
#include "pb.h"

// Static instance pointer initialization (used for MQTT callback context)
AkitaSmartCityServices* AkitaSmartCityServices::s_instance = nullptr;

// --- Nanopb Map Field Callback Implementations ---

/**
 * @brief Helper function to encode a std::string for a nanopb CALLBACK string field.
 * This is used within the map encoding callback.
 * @param stream Nanopb output stream.
 * @param field Field descriptor.
 * @param arg Pointer to a pointer to the std::string to encode.
 * @return True on success, false on failure.
 */
bool pb_encode_string_helper(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    const std::string* str = static_cast<const std::string*>(*arg);
    if (!str) return false; // Check for null pointer
    if (!pb_encode_tag_for_field(stream, field)) return false;
    return pb_encode_string(stream, reinterpret_cast<const pb_byte_t*>(str->c_str()), str->length());
}

/**
 * @brief Helper function to decode a std::string for a nanopb CALLBACK string field.
 * This is used within the map decoding callback.
 * @param stream Nanopb input stream.
 * @param field Field descriptor.
 * @param arg Pointer to a pointer to the std::string to decode into.
 * @return True on success, false on failure.
 */
bool pb_decode_string_helper(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    std::string* str = static_cast<std::string*>(*arg);
    if (!str) return false; // Check for null pointer

    size_t len = stream->bytes_left;
    try {
        str->resize(len); // Allocate space
    } catch (const std::bad_alloc& e) {
        Log.printf(LOG_LEVEL_ERROR, "ASCS Nanopb: Failed to allocate memory for string decode (%d bytes)\n", len);
        return false; // Memory allocation failed
    }

    if (!pb_read(stream, reinterpret_cast<pb_byte_t*>(&(*str)[0]), len)) {
        Log.printf(LOG_LEVEL_ERROR, "ASCS Nanopb: Failed to read string data (%d bytes)\n", len);
        return false; // Reading from stream failed
    }
    return true;
}

/**
 * @brief Nanopb ENCODE callback for map<string, float> fields.
 * This function is called repeatedly by pb_encode to serialize map entries.
 * It expects the 'arg' to point to a MapCallbackContext containing the map and iterator state.
 */
bool AkitaSmartCityServices::encode_map_callback(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    MapCallbackContext* context = static_cast<MapCallbackContext*>(*arg);
    // Basic validation of context and map pointer
    if (!context || !context->map_ptr) {
        Log.println(LOG_LEVEL_ERROR, "ASCS Nanopb Encode Map: Invalid context or map pointer.");
        return false;
    }

    std::map<std::string, float>& map_to_encode = *context->map_ptr;

    // Initialize iterator on the first call for this specific map field encoding process
    if (stream->state == nullptr) {
        Log.println(LOG_LEVEL_VERBOSE, "ASCS Nanopb Encode Map: Starting encoding process.");
        context->map_iterator = map_to_encode.begin();
        stream->state = context; // Use the context address as a state marker for subsequent calls
        context->encode_successful = true; // Reset success flag for this encoding run
    }

    // Iterate through the map elements as long as encoding is successful
    while (context->encode_successful && context->map_iterator != map_to_encode.end()) {
        // Define the structure of the submessage (map entry) - must match the implicit structure expected by map fields
        // message MapStringFloatEntry { string key = 1; float value = 2; }
        struct Entry {
            pb_callback_t key; // Key uses string callback
            float value;
        };
        pb_field_t entry_fields[] = {
            // Field 1: Key (string, required, uses callback)
            PB_FIELD(  1, STRING  , REQUIRED, CALLBACK, 0, 0, 0),
            // Field 2: Value (float, required, static data type, relative offset calculated)
            PB_FIELD(  2, FLOAT   , REQUIRED, STATIC  , OTHER, Entry, key, 0),
            PB_LAST_FIELD // Marks the end of the field list
        };

        // Prepare the data structure for the current map entry
        Entry entry_data;
        entry_data.key.funcs.encode = pb_encode_string_helper; // Assign string encoding helper
        entry_data.key.arg = (void*)&(context->map_iterator->first); // Pass pointer to the current key string
        entry_data.value = context->map_iterator->second; // Assign the current value

        Log.printf(LOG_LEVEL_VERBOSE, "ASCS Nanopb Encode Map: Encoding entry: Key='%s', Value=%.2f\n",
                   context->map_iterator->first.c_str(), entry_data.value);

        // Encode the tag for the map field itself (this happens repeatedly for each entry)
        if (!pb_encode_tag_for_field(stream, field)) {
            Log.println(LOG_LEVEL_ERROR, "ASCS Nanopb Encode Map: Failed to encode map field tag.");
            context->encode_successful = false; // Mark failure
            break; // Stop encoding this map
        }

        // Encode the current map entry as a submessage
        if (!pb_encode_submessage(stream, entry_fields, &entry_data)) {
            Log.printf(LOG_LEVEL_ERROR, "ASCS Nanopb Encode Map: Failed to encode map entry submessage: %s\n", PB_GET_ERROR(stream));
            context->encode_successful = false; // Mark failure
            break; // Stop encoding this map
        }

        // Move to the next element in the map
        ++(context->map_iterator);
    }

    // Clean up state marker on the last call for this map field
    if (context->map_iterator == map_to_encode.end()) {
        Log.println(LOG_LEVEL_VERBOSE, "ASCS Nanopb Encode Map: Finished encoding process.");
        stream->state = nullptr; // Clear state marker
    }

    // Return the overall success status for this encoding run
    return context->encode_successful;
}


/**
 * @brief Nanopb DECODE callback for map<string, float> fields.
 * This function is called repeatedly by pb_decode for each map entry submessage found in the stream.
 * It expects the 'arg' to point to a MapCallbackContext containing the map to populate.
 */
bool AkitaSmartCityServices::decode_map_callback(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    MapCallbackContext* context = static_cast<MapCallbackContext*>(*arg);
    // Basic validation of context and map pointer
    if (!context || !context->map_ptr) {
        Log.println(LOG_LEVEL_ERROR, "ASCS Nanopb Decode Map: Invalid context or map pointer.");
        return false;
    }

    std::map<std::string, float>& map_to_decode = *context->map_ptr;

    // Define the structure of the submessage (map entry) we expect to decode
    // message MapStringFloatEntry { string key = 1; float value = 2; }
    struct Entry {
        pb_callback_t key; // Key uses string callback
        float value;
    };
    pb_field_t entry_fields[] = {
        PB_FIELD(  1, STRING  , REQUIRED, CALLBACK, 0, 0, 0),
        PB_FIELD(  2, FLOAT   , REQUIRED, STATIC  , OTHER, Entry, key, 0),
        PB_LAST_FIELD
    };

    // Prepare data structure to hold the decoded entry
    Entry entry_data;
    std::string current_key; // Temporary string to store the decoded key
    entry_data.key.funcs.decode = pb_decode_string_helper; // Assign string decoding helper
    entry_data.key.arg = &current_key; // Pass pointer to the temporary key string
    entry_data.value = 0.0f; // Initialize value

    // Decode the submessage from the stream
    if (!pb_decode(stream, entry_fields, &entry_data)) {
        Log.printf(LOG_LEVEL_ERROR, "ASCS Nanopb Decode Map: Failed to decode map entry submessage: %s\n", PB_GET_ERROR(stream));
        return false; // Decoding failed
    }

    // Add the successfully decoded key-value pair to the target map
    // Use operator[] for convenience (inserts if key doesn't exist, updates if it does)
    try {
        map_to_decode[current_key] = entry_data.value;
    } catch (const std::bad_alloc& e) {
        Log.printf(LOG_LEVEL_ERROR, "ASCS Nanopb Decode Map: Failed to allocate memory for map insertion (Key: %s)\n", current_key.c_str());
        return false; // Memory allocation failed
    }


    Log.printf(LOG_LEVEL_VERBOSE, "ASCS Nanopb Decode Map: Decoded entry: Key='%s', Value=%.2f\n",
               current_key.c_str(), entry_data.value);

    return true; // Successfully decoded this entry
}


// --- Constructor / Destructor ---

AkitaSmartCityServices::AkitaSmartCityServices(const char *name) : MeshtasticPlugin(name) {
    // Initialize pointers to null
    m_wifiClient = nullptr;
    m_mqttClient = nullptr;
    s_instance = this; // Set static instance pointer for MQTT callback
}

AkitaSmartCityServices::~AkitaSmartCityServices() {
    // Clean up dynamically allocated resources
    delete m_mqttClient;
    delete m_wifiClient;
    // unique_ptr for m_sensor handles its own deletion
    if (s_instance == this) {
        s_instance = nullptr; // Clear static instance if this was the one
    }
}

// --- Meshtastic Plugin API Methods ---

void AkitaSmartCityServices::init(const MeshtasticAPI *api) {
    m_api = api;
    Log.printf(LOG_LEVEL_INFO, "[%s] Initializing...\n", getName()); // Use [] for plugin name for clarity

    // Load configuration using the config manager
    m_config.load();

    Log.printf(LOG_LEVEL_INFO, "[%s] Config: Role=%d, ServiceID=%lu, TargetNode=0x%lx, ReadInterval=%lums, DiscoveryInterval=%lums\n",
               getName(), m_config.getNodeRole(), m_config.getServiceId(), m_config.getTargetNodeId(),
               m_config.getSensorReadIntervalMs(), m_config.getDiscoveryIntervalMs());

    // Initialize network clients and filesystem if this node is a Gateway
    if (m_config.getNodeRole() == ServiceDiscovery_Role_GATEWAY) {
        #ifdef ASCS_ROLE_GATEWAY
            Log.println(LOG_LEVEL_INFO, "[%s] Initializing Gateway components...", getName());
            // Allocate network clients
            try {
                m_wifiClient = new WiFiClient();
                m_mqttClient = new PubSubClient(*m_wifiClient);
            } catch (const std::bad_alloc& e) {
                 Log.println(LOG_LEVEL_CRITICAL, "[%s] Failed to allocate memory for network clients!", getName());
                 // Cannot proceed as Gateway without clients
                 return;
            }

            m_mqttClient->setServer(m_config.getMqttServer().c_str(), m_config.getMqttPort());
            m_mqttClient->setCallback(mqttCallback); // Set static callback
            // Increase MQTT buffer size if needed for larger JSON payloads
            // m_mqttClient->setBufferSize(512); // Example: 512 bytes

            // Initialize Filesystem for buffering
            // Note: Filesystem must be initialized *before* first use (e.g., in main setup())
            // We check if it's mounted here, assuming it was initialized earlier.
            if (!FileSystem.exists("/")) { // Basic check if filesystem seems usable
                 Log.println(LOG_LEVEL_ERROR, "[%s] Filesystem not mounted! Gateway buffering disabled.", getName());
            } else {
                 Log.println(LOG_LEVEL_INFO, "[%s] Filesystem ready for buffering.", getName());
                 // Optional: Check buffer file size, potentially clear if corrupted or too large on boot?
                 // File file = FileSystem.open(ASCS_GATEWAY_BUFFER_FILENAME, FILE_READ);
                 // if (file && file.size() > ASCS_GATEWAY_BUFFER_MAX_SIZE * 1.1) { // Example check
                 //    Log.println(LOG_LEVEL_WARNING, "[%s] Buffer file seems too large, clearing.", getName());
                 //    file.close();
                 //    FileSystem.remove(ASCS_GATEWAY_BUFFER_FILENAME);
                 // } else if (file) {
                 //    file.close();
                 // }
            }

            connectWiFi(); // Initial connection attempt (can block briefly)
        #else
            // This code block will only be reached if the role is set to Gateway in preferences,
            // but the ASCS_ROLE_GATEWAY flag was NOT set during compilation.
            Log.println(LOG_LEVEL_ERROR, "[%s] Gateway role configured but support not compiled in! Check ASCS_ROLE_GATEWAY flag.", getName());
            // Node cannot function as a gateway. Consider setting role to UNKNOWN or logging critical error.
        #endif
    }

    // Send initial service discovery announcement regardless of role
    sendServiceDiscovery();
    m_lastDiscoverySendTime = millis();
    m_lastServiceCleanupTime = millis();
    m_lastBufferProcessTime = millis(); // Initialize buffer processing timer

    Log.printf(LOG_LEVEL_INFO, "[%s] Initialization complete.\n", getName());
}

void AkitaSmartCityServices::loop() {
    unsigned long now = millis();
    bool work_done = false; // Flag if loop performed significant action

    // --- Feed Watchdog Timer ---
    // It's crucial to feed the watchdog regularly, especially before/after
    // potentially long-running operations. The exact call depends on your setup.
    // Example: esp_task_wdt_reset();
    // feed_watchdog_placeholder();

    // --- Role-Specific Periodic Actions ---
    ServiceDiscovery_Role current_role = m_config.getNodeRole();

    if (current_role == ServiceDiscovery_Role_SENSOR && m_sensor != nullptr) {
        if (now - m_lastSensorReadTime >= m_config.getSensorReadIntervalMs()) {
            runSensorLogic();
            m_lastSensorReadTime = now;
            work_done = true;
        }
    } else if (current_role == ServiceDiscovery_Role_GATEWAY) {
        #ifdef ASCS_ROLE_GATEWAY
            // Check network connections periodically
            checkWiFiConnection();
            checkMQTTConnection(); // Handles reconnection attempts

            // Process MQTT messages if connected
            if (m_mqttClient && m_mqttClient->connected()) {
                 if(m_mqttClient->loop()) work_done = true; // Let MQTT client handle keepalives, incoming messages

                 // Process the message buffer if MQTT is connected
                 // Check reasonably often, but not necessarily every loop iteration
                 if (now - m_lastBufferProcessTime > 5000) { // Check buffer every 5 seconds
                     processBufferedPackets(); // Attempt to send one buffered packet
                     m_lastBufferProcessTime = now;
                     work_done = true; // Assume buffer processing is work
                 }
            }
        #endif
    }
    // Aggregator role primarily reacts to incoming packets in handleReceived()

    // --- General Periodic Actions ---

    // Periodic Service Discovery broadcast
    if (now - m_lastDiscoverySendTime >= m_config.getDiscoveryIntervalMs()) {
        sendServiceDiscovery();
        m_lastDiscoverySendTime = now;
        work_done = true;
    }

    // Periodic Service Table cleanup
    // Run cleanup slightly more often than the timeout to prevent excessive buildup
    if (now - m_lastServiceCleanupTime >= (m_config.getServiceTimeoutMs() / 2)) {
        cleanupServiceTable();
        m_lastServiceCleanupTime = now;
        work_done = true;
    }

    // --- Feed Watchdog Again (Optional) ---
    // If the loop did significant work, feeding again ensures responsiveness
    // if (work_done) {
    //    feed_watchdog_placeholder();
    // }
}

/**
 * @brief Handles incoming Meshtastic packets, checks port, decodes, and routes.
 */
bool AkitaSmartCityServices::handleReceived(const meshPacket &packet) {
    // Ignore packets not intended for our specific application port
    if (packet.decoded.portnum != ASCS_PORT_NUM) {
        return false; // Not our packet
    }

    Log.printf(LOG_LEVEL_DEBUG, "[%s] Received packet on port %d from 0x%lx, size %d, RSSI %d, SNR %.2f\n",
               getName(), ASCS_PORT_NUM, packet.from, packet.decoded.payloadlen,
               packet.rx_rssi, packet.rx_snr); // Log signal quality

    // Prepare for decoding
    SmartCityPacket scp = SmartCityPacket_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(packet.decoded.payload, packet.decoded.payloadlen);

    // Prepare context for decoding the map field if the payload is SensorData
    MapCallbackContext decode_context;
    std::map<std::string, float> decoded_readings; // Temporary map to store decoded readings
    decode_context.map_ptr = &decoded_readings;

    // Assign the decode callback function to the 'readings' field descriptor within the packet structure
    // This tells nanopb to use our function when it encounters the 'readings' field (tag 3) inside SensorData.
    scp.payload.sensor_data.readings.funcs.decode = decode_map_callback;
    scp.payload.sensor_data.readings.arg = &decode_context; // Pass our context struct

    // Attempt to decode the main SmartCityPacket
    if (pb_decode(&stream, SmartCityPacket_fields, &scp)) {
        Log.println(LOG_LEVEL_DEBUG, "[%s] Successfully decoded SmartCityPacket", getName());

        // Packet decoded successfully, handle based on payload type
        switch (scp.which_payload) {
            case SmartCityPacket_discovery_tag:
                Log.printf(LOG_LEVEL_DEBUG, "[%s] Handling ServiceDiscovery from 0x%lx\n", getName(), packet.from);
                handleServiceDiscovery(scp.payload.discovery, packet.from);
                break;

            case SmartCityPacket_sensor_data_tag:
                // SensorData payload was decoded into scp.payload.sensor_data
                // The map field 'readings' was populated into 'decoded_readings' via the callback.
                // Now, handle the received sensor data, potentially passing the decoded map along.
                Log.printf(LOG_LEVEL_DEBUG, "[%s] Handling SensorData from 0x%lx (Map size: %d)\n",
                           getName(), packet.from, decoded_readings.size());

                // ** Important: Pass the decoded map data to the handler **
                // Modify handleSensorData or subsequent functions (runGatewayLogic etc.)
                // to accept the decoded map for processing (e.g., MQTT publishing).
                // For now, we call the existing handleSensorData which only takes the base struct.
                // This means the map data isn't directly used downstream yet in this structure.
                // --> Refactoring needed to pass 'decoded_readings' down if required by MQTT publishing etc. <--
                handleSensorData(scp.payload.sensor_data, packet.from);
                break;

            // case SmartCityPacket_config_tag: // Placeholder for future remote config
            //     Log.printf(LOG_LEVEL_DEBUG, "[%s] Handling ServiceConfig from 0x%lx\n", getName(), packet.from);
            //     // handleServiceConfig(scp.payload.config, packet.from);
            //     break;

            default:
                // This case handles unknown or potentially corrupted payload types within a valid packet structure.
                Log.printf(LOG_LEVEL_WARNING, "[%s] Received unknown payload type (%d) in SmartCityPacket from 0x%lx\n", getName(), scp.which_payload, packet.from);
                break;
        }
        return true; // Packet was successfully decoded and handled (or identified as unknown type)

    } else {
        // Decoding failed, log the error. The packet might be corrupted or not match the expected format.
        Log.printf(LOG_LEVEL_ERROR, "[%s] Failed to decode SmartCityPacket from 0x%lx: %s\n",
                   getName(), packet.from, PB_GET_ERROR(&stream));
        return false; // Indicate decoding failure
    }
}

// --- Public Configuration Methods ---

/**
 * @brief Assigns the sensor implementation object.
 */
void AkitaSmartCityServices::setSensor(std::unique_ptr<SensorInterface> sensor) {
    m_sensor = std::move(sensor); // Take ownership
    if (m_sensor) {
        Log.println(LOG_LEVEL_INFO, "[%s] Sensor implementation set.", getName());
    } else {
        Log.println(LOG_LEVEL_WARNING, "[%s] Sensor implementation set to null.", getName());
    }
}

/**
 * @brief Returns the currently configured node role.
 */
ServiceDiscovery_Role AkitaSmartCityServices::getNodeRole() const {
    return m_config.getNodeRole();
}

// --- Internal Helper Methods ---

#ifdef ASCS_ROLE_GATEWAY
/**
 * @brief Connects to WiFi using configured credentials. Blocking with timeout.
 */
void AkitaSmartCityServices::connectWiFi() {
    // Skip if already connected
    if (WiFi.status() == WL_CONNECTED) return;

    Log.printf(LOG_LEVEL_INFO, "[%s] Connecting to WiFi SSID: %s\n", getName(), m_config.getWifiSsid().c_str());
    WiFi.mode(WIFI_STA); // Ensure Station mode
    WiFi.begin(m_config.getWifiSsid().c_str(), m_config.getWifiPassword().c_str());

    unsigned long startAttemptTime = millis();
    // Wait for connection, but with a timeout to prevent indefinite blocking
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 20000) { // 20 second timeout
        // feed_watchdog_placeholder(); // Feed watchdog during potentially long loop
        delay(500); // Short delay between checks
        Serial.print("."); // Visual feedback on serial console
    }
    Serial.println(); // Newline after dots

    if (WiFi.status() == WL_CONNECTED) {
        Log.printf(LOG_LEVEL_INFO, "[%s] WiFi connected. IP: %s\n", getName(), WiFi.localIP().toString().c_str());
        // Attempt MQTT connection now that WiFi is established
        connectMQTT();
    } else {
        Log.println(LOG_LEVEL_ERROR, "[%s] WiFi connection failed!", getName());
        WiFi.disconnect(true); // Disconnect explicitly
        WiFi.mode(WIFI_OFF);   // Turn off WiFi radio to save power if connection failed
    }
}

/**
 * @brief Checks WiFi connection status and attempts reconnect if disconnected.
 */
void AkitaSmartCityServices::checkWiFiConnection() {
     // This function should only run on gateway nodes
     if (m_config.getNodeRole() != ServiceDiscovery_Role_GATEWAY) return;

     if (WiFi.status() != WL_CONNECTED) {
         Log.println(LOG_LEVEL_WARNING, "[%s] WiFi disconnected. Attempting reconnect...", getName());
         // Consider adding a backoff mechanism here to avoid constant reconnection attempts
         // e.g., only try every 30 seconds if the last attempt failed.
         connectWiFi(); // Attempt to reconnect
     }
}

/**
 * @brief Connects to the MQTT broker using configured credentials. Non-blocking attempt.
 */
void AkitaSmartCityServices::connectMQTT() {
    // Skip if client not initialized or already connected
    if (!m_mqttClient || m_mqttClient->connected()) return;
    // Skip if WiFi is not available
    if (WiFi.status() != WL_CONNECTED) {
        Log.println(LOG_LEVEL_WARNING, "[%s] Cannot connect MQTT, WiFi is down.", getName());
        return;
    }

    Log.printf(LOG_LEVEL_INFO, "[%s] Attempting MQTT connection to %s:%d...\n", getName(), m_config.getMqttServer().c_str(), m_config.getMqttPort());

    // Create a unique client ID for this node
    String clientId = "meshtastic-ascs-";
    clientId += String(m_api->getMyNodeInfo()->node_num, HEX); // Use Meshtastic node ID

    // Get credentials from config
    std::string user = m_config.getMqttUser();
    std::string pass = m_config.getMqttPassword();
    bool result;

    // Set buffer size if needed BEFORE connecting (adjust based on expected payload size)
    // m_mqttClient->setBufferSize(512);

    // Attempt connection with or without authentication based on config
    if (!user.empty()) {
        result = m_mqttClient->connect(clientId.c_str(), user.c_str(), pass.c_str());
    } else {
        result = m_mqttClient->connect(clientId.c_str());
    }

    if (result) {
        Log.println(LOG_LEVEL_INFO, "[%s] MQTT connected.", getName());
        m_gatewayBufferActive = false; // Clear buffer flag on successful connect
        m_lastBufferProcessTime = millis(); // Trigger buffer processing check soon
        // Subscribe to any command topics if needed
        // Example: m_mqttClient->subscribe("akita/smartcity/gateway/+/command");
    } else {
        // Log detailed error based on PubSubClient state code
        Log.printf(LOG_LEVEL_ERROR, "[%s] MQTT connection failed, rc=%d. Check server, port, credentials, client ID, and MQTT buffer size. Will retry later.\n", getName(), m_mqttClient->state());
        m_gatewayBufferActive = true; // Activate buffering since connection failed
    }
    // Record the time of this attempt, regardless of success, for reconnection scheduling
    m_lastMqttReconnectAttempt = millis();
}

/**
 * @brief Checks MQTT connection status and attempts reconnect periodically if disconnected.
 */
void AkitaSmartCityServices::checkMQTTConnection() {
    if (m_config.getNodeRole() != ServiceDiscovery_Role_GATEWAY || !m_mqttClient) return;

    if (!m_mqttClient->connected()) {
        // Ensure buffering is active if we detect disconnection
        if (!m_gatewayBufferActive) {
             Log.println(LOG_LEVEL_WARNING, "[%s] MQTT detected disconnected, activating buffering.", getName());
             m_gatewayBufferActive = true;
        }

        unsigned long now = millis();
        // Only attempt reconnection periodically based on configured interval
        if (now - m_lastMqttReconnectAttempt > m_config.getMqttReconnectIntervalMs()) {
             Log.println(LOG_LEVEL_WARNING, "[%s] MQTT disconnected. Attempting periodic reconnect...", getName());
             connectMQTT(); // Attempt reconnection (updates m_lastMqttReconnectAttempt)
        }
    } else {
        // MQTT is connected
        // If we were buffering previously, log that we've reconnected and stopped buffering.
        // The actual processing of the buffer happens in the main loop based on m_lastBufferProcessTime.
        if (m_gatewayBufferActive) {
             Log.println(LOG_LEVEL_INFO, "[%s] MQTT reconnected, buffering stopped (pending buffer processing).", getName());
             m_gatewayBufferActive = false; // Allow direct publishing again
        }
    }
    // Note: m_mqttClient->loop() is called in the main plugin loop() to handle keepalives etc.
}


/**
 * @brief Static MQTT message callback handler. Required by PubSubClient.
 * Routes the call to the instance method if available.
 */
void AkitaSmartCityServices::mqttCallback(char *topic, byte *payload, unsigned int length) {
    // Use the static instance pointer to call a non-static handler if possible
    if (s_instance) {
        Log.printf(LOG_LEVEL_INFO, "[%s] MQTT message received on topic: %s\n", s_instance->getName(), topic);
        // Ensure payload is null-terminated for safe string processing
        payload[length] = '\0';
        std::string message((char*)payload);
        Log.printf(LOG_LEVEL_DEBUG, "[%s] MQTT Payload: %s\n", s_instance->getName(), message.c_str());

        // --- TODO: Implement Downlink Command Handling ---
        // Example: Parse topic/payload (e.g., JSON command) and potentially send
        // a message back into the mesh network via s_instance->sendMessage(...)
        // if (strstr(topic, "/command")) { ... parse command ... }
    } else {
        // This should not happen if the plugin is initialized correctly
        Log.println(LOG_LEVEL_ERROR, "MQTT Callback: Static instance pointer is null!");
    }
}
#else
// Provide empty stubs for WiFi/MQTT functions if Gateway role support is not compiled in.
// This prevents linker errors if the role is set to Gateway in preferences but the
// necessary code wasn't included via the ASCS_ROLE_GATEWAY build flag.
void AkitaSmartCityServices::connectWiFi() {}
void AkitaSmartCityServices::checkWiFiConnection() {}
void AkitaSmartCityServices::connectMQTT() {}
void AkitaSmartCityServices::checkMQTTConnection() {}
void AkitaSmartCityServices::mqttCallback(char*, byte*, unsigned int) {}
#endif // ASCS_ROLE_GATEWAY

/**
 * @brief Handles received ServiceDiscovery messages. Updates the local service table.
 */
void AkitaSmartCityServices::handleServiceDiscovery(const ServiceDiscovery &discovery, uint32_t fromNode) {
    // Update our table of known nodes and their advertised roles/services
    updateServiceTable(fromNode, discovery.node_role, discovery.service_id);
}

/**
 * @brief Handles received SensorData messages. Routes to role-specific logic.
 */
void AkitaSmartCityServices::handleSensorData(const SensorData &sensorData, uint32_t fromNode) {
    // Create the full packet wrapper to pass to role-specific handlers
    // This ensures Aggregators/Gateways have the complete packet for forwarding/buffering.
    SmartCityPacket packet = SmartCityPacket_init_zero;
    packet.which_payload = SmartCityPacket_sensor_data_tag;
    packet.payload.sensor_data = sensorData; // Copy the received sensor data

    // Route based on the role of *this* node
    switch (m_config.getNodeRole()) {
        case ServiceDiscovery_Role_AGGREGATOR:
            runAggregatorLogic(packet, fromNode); // Pass the full packet
            break;
        case ServiceDiscovery_Role_GATEWAY:
            runGatewayLogic(packet, fromNode); // Pass the full packet
            break;
        case ServiceDiscovery_Role_SENSOR:
            // Sensors typically don't process sensor data from others, but log it.
            Log.printf(LOG_LEVEL_DEBUG, "[%s] Sensor node received unexpected SensorData from 0x%lx\n", getName(), fromNode);
            break;
        default: // UNKNOWN role
            Log.printf(LOG_LEVEL_DEBUG, "[%s] Node with UNKNOWN role received SensorData from 0x%lx\n", getName(), fromNode);
            break;
    }
}

/**
 * @brief Encodes and sends a SmartCityPacket over the Meshtastic network.
 * @param toNode Destination Node ID (use ASCS_BROADCAST_ADDR for broadcast).
 * @param packet The SmartCityPacket to send (must have callbacks set if map data is present).
 * @return True if the packet was successfully queued for transmission, false otherwise.
 */
bool AkitaSmartCityServices::sendMessage(uint32_t toNode, const SmartCityPacket &packet) {
    // Allocate buffer for the encoded packet
    uint8_t buffer[ASCS_GATEWAY_MAX_PACKET_SIZE]; // Use defined max size for consistency
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    // --- Nanopb Encoding ---
    // IMPORTANT: If the packet contains SensorData with a map, the calling function
    // (e.g., runSensorLogic) MUST have already set the .funcs.encode and .arg fields
    // on the packet.payload.sensor_data.readings field before calling sendMessage.
    // This function assumes the packet is fully prepared for encoding.

    if (pb_encode(&stream, SmartCityPacket_fields, &packet)) {
        // Encoding successful
        size_t encoded_len = stream.bytes_written;
        Log.printf(LOG_LEVEL_DEBUG, "[%s] Sending packet (type %d) to 0x%lx, size %d bytes\n",
                   getName(), packet.which_payload, toNode, encoded_len);

        // Sanity check encoded size
        if (encoded_len == 0 || encoded_len > ASCS_GATEWAY_MAX_PACKET_SIZE) {
            Log.printf(LOG_LEVEL_ERROR, "[%s] Invalid encoded packet size (%d)!\n", getName(), encoded_len);
            return false;
        }

        // Get the primary Meshtastic interface to send data
        MeshInterface *iface = m_api->getPrimaryInterface();
        if (iface) {
            // --- Watchdog Feed ---
            // Feed watchdog before potentially blocking radio transmission
            // feed_watchdog_placeholder();

            // Send the data using the Meshtastic API
            // Use default ACK behavior (usually WANT_ACK=1 for directed messages)
            // Hop Limit 0 usually means use default (e.g., 3 hops)
            bool success = iface->sendData(toNode, buffer, encoded_len, ASCS_PORT_NUM, Data_WANT_ACK_DEFAULT, 0);

            // --- Watchdog Feed ---
            // Feed watchdog again after transmission attempt
            // feed_watchdog_placeholder();

            if (!success) {
                Log.println(LOG_LEVEL_WARNING, "[%s] Meshtastic sendData failed (queue full or radio busy?).", getName());
            }
            return success; // Return status from sendData

        } else {
            Log.println(LOG_LEVEL_ERROR, "[%s] Failed to get primary mesh interface!", getName());
            return false; // Cannot send without interface
        }

    } else {
        // Encoding failed
        Log.printf(LOG_LEVEL_ERROR, "[%s] Failed to encode SmartCityPacket: %s\n", getName(), PB_GET_ERROR(&stream));
        return false;
    }
}

/**
 * @brief Sends a Service Discovery announcement packet.
 * @param toNode Destination address (defaults to broadcast).
 */
void AkitaSmartCityServices::sendServiceDiscovery(uint32_t toNode /*= ASCS_BROADCAST_ADDR*/) {
    Log.printf(LOG_LEVEL_DEBUG, "[%s] Sending Service Discovery to 0x%lx\n", getName(), toNode);
    // Create the packet payload
    SmartCityPacket packet = SmartCityPacket_init_zero;
    packet.which_payload = SmartCityPacket_discovery_tag;
    packet.payload.discovery.node_role = m_config.getNodeRole();
    packet.payload.discovery.service_id = m_config.getServiceId();

    // Send the packet
    sendMessage(toNode, packet);
}

/**
 * @brief Sends sensor data, determining the destination automatically if not configured.
 * Assumes the SensorData struct has been fully prepared (including map callbacks if needed).
 * @param sensorData The prepared SensorData message to send.
 */
void AkitaSmartCityServices::sendSensorData(const SensorData &sensorData) {
    // Determine the target node ID
    uint32_t target = m_config.getTargetNodeId();

    // If no specific target is configured (0), try to find a gateway via discovery
    if (target == 0 || target == ASCS_BROADCAST_ADDR) {
        target = findGatewayNode(); // Find best known gateway from service table
        if (target != 0) {
            Log.printf(LOG_LEVEL_DEBUG, "[%s] No target configured, using discovered Gateway 0x%lx\n", getName(), target);
        }
    }

    // If still no target found (no specific config, no gateway discovered), broadcast
    if (target == 0) {
        target = ASCS_BROADCAST_ADDR;
        Log.println(LOG_LEVEL_DEBUG, "[%s] No gateway found, broadcasting sensor data.", getName());
    }

    // Create the packet wrapper
    SmartCityPacket packet = SmartCityPacket_init_zero;
    packet.which_payload = SmartCityPacket_sensor_data_tag;
    // Assign the already-prepared SensorData struct (MUST have callbacks set by caller if map used)
    packet.payload.sensor_data = sensorData;

    // Send the packet
    sendMessage(target, packet);
}


// --- Role-Specific Logic ---

/**
 * @brief Performs actions for the Sensor role: reads sensor, prepares data, sends.
 */
void AkitaSmartCityServices::runSensorLogic() {
    // Check if a sensor implementation has been provided
    if (!m_sensor) {
        Log.println(LOG_LEVEL_WARNING, "[%s] Sensor role active, but no sensor implementation provided!", getName());
        return;
    }

    Log.println(LOG_LEVEL_DEBUG, "[%s] Reading sensor data...", getName());
    SensorData data = SensorData_init_zero; // Initialize proto struct
    std::map<std::string, float> readingsMap; // Temporary map to hold readings

    // --- Watchdog Feed ---
    // Feed before potentially long sensor read
    // feed_watchdog_placeholder();

    // Attempt to read data from the sensor implementation
    bool read_success = false;
    try {
        read_success = m_sensor->readData(readingsMap);
    } catch (const std::exception& e) {
         Log.printf(LOG_LEVEL_ERROR, "[%s] Exception during sensor read: %s\n", getName(), e.what());
         read_success = false;
    } catch (...) {
          Log.println(LOG_LEVEL_ERROR, "[%s] Unknown exception during sensor read!", getName());
          read_success = false;
    }


    // --- Watchdog Feed ---
    // Feed after potentially long sensor read
    // feed_watchdog_placeholder();

    if (read_success) {
        Log.printf(LOG_LEVEL_DEBUG, "[%s] Sensor read successful (%d readings).\n", getName(), readingsMap.size());

        // Populate standard SensorData fields
        strncpy(data.sensor_id, m_sensor->getSensorId().c_str(), sizeof(data.sensor_id) - 1);
        data.sensor_id[sizeof(data.sensor_id) - 1] = '\0'; // Ensure null termination

        // Use Meshtastic's time (can be GPS or RTC synchronized)
        data.timestamp_utc = m_api->getAdjustedTime();
        // Increment sequence number for this sensor node
        data.sequence_num = ++m_sensorSequenceNum;

        // ** Prepare the map field for encoding **
        MapCallbackContext encode_context;
        encode_context.map_ptr = &readingsMap; // Point context to our map containing the readings
        data.readings.funcs.encode = encode_map_callback; // Set the callback function for the 'readings' field
        data.readings.arg = &encode_context; // Pass our context struct as the argument

        // Now the 'data' struct is fully prepared, including the setup for map encoding.
        // Send the prepared SensorData.
        sendSensorData(data);

    } else {
        Log.println(LOG_LEVEL_ERROR, "[%s] Failed to read sensor data.", getName());
        // Consider sending a status message indicating sensor failure?
    }
}

/**
 * @brief Performs actions for the Aggregator role: forwards received sensor packets.
 * @param packet The full SmartCityPacket containing SensorData received from another node.
 * @param fromNode The Node ID of the original sender.
 */
void AkitaSmartCityServices::runAggregatorLogic(const SmartCityPacket &packet, uint32_t fromNode) {
    Log.printf(LOG_LEVEL_INFO, "[%s] Aggregator received sensor data from 0x%lx.\n", getName(), fromNode);

    // Determine the target gateway
    uint32_t targetGateway = m_config.getTargetNodeId(); // Use configured target first
    if (targetGateway == 0 || targetGateway == ASCS_BROADCAST_ADDR) {
        targetGateway = findGatewayNode(); // Try to find one via discovery
        if (targetGateway != 0) {
            Log.printf(LOG_LEVEL_DEBUG, "[%s] Aggregator using discovered gateway 0x%lx\n", getName(), targetGateway);
        }
    }

    // Forward the packet if a target gateway is known
    if (targetGateway != 0 && targetGateway != ASCS_BROADCAST_ADDR) {
        Log.printf(LOG_LEVEL_INFO, "[%s] Aggregator forwarding data from 0x%lx to Gateway 0x%lx\n", getName(), fromNode, targetGateway);
        // Forward the *exact same* packet received.
        // Assumes the packet is ready for re-transmission (map callbacks might need reset if re-encoding).
        // For simple forwarding, sending the original encoded bytes might be more efficient if possible,
        // but requires modifying handleReceived and sendMessage. Sending the decoded packet is simpler.
        sendMessage(targetGateway, packet);
    } else {
        // No target gateway known, drop the packet to avoid broadcast storms.
        Log.printf(LOG_LEVEL_WARNING, "[%s] Aggregator received data from 0x%lx, but no target gateway known. Dropping.\n", getName(), fromNode);
    }
}

/**
 * @brief Performs actions for the Gateway role: publishes or buffers received sensor packets.
 * @param packet The full SmartCityPacket containing SensorData received from another node.
 * @param fromNode The Node ID of the original sender.
 */
void AkitaSmartCityServices::runGatewayLogic(const SmartCityPacket &packet, uint32_t fromNode) {
    Log.printf(LOG_LEVEL_INFO, "[%s] Gateway received sensor data from 0x%lx.\n", getName(), fromNode);

    #ifdef ASCS_ROLE_GATEWAY
        // Pass the packet and originating node ID to the publish/buffer logic
        publishMqttOrBuffer(packet, fromNode);
    #else
        // Should not happen if role check is done correctly, but log defensively.
        Log.println(LOG_LEVEL_WARNING, "[%s] Gateway logic called, but support not compiled in!", getName());
    #endif
}

// --- Service Discovery Management ---

/**
 * @brief Updates the local table of discovered services/nodes.
 * @param nodeId The Node ID of the discovered node.
 * @param role The advertised role of the node.
 * @param serviceId The advertised service ID of the node.
 */
void AkitaSmartCityServices::updateServiceTable(uint32_t nodeId, ServiceDiscovery_Role role, uint32_t serviceId) {
    // Ignore discovery messages from ourselves
    if (nodeId == m_api->getMyNodeInfo()->node_num) return;

    unsigned long now = millis();
    // Use operator[] to insert or update the entry for the given nodeId
    m_serviceTable[nodeId] = {role, serviceId, now};
    Log.printf(LOG_LEVEL_DEBUG, "[%s] Updated service table for node 0x%lx: Role=%d, ServiceID=%lu, LastSeen=%lu\n",
               getName(), nodeId, role, serviceId, now);
}

/**
 * @brief Removes stale entries from the service table based on the configured timeout.
 */
void AkitaSmartCityServices::cleanupServiceTable() {
    unsigned long now = millis();
    int removedCount = 0;
    // Iterate through the map and erase elements that have timed out
    for (auto it = m_serviceTable.begin(); it != m_serviceTable.end(); /* no increment here */) {
        // Check if the entry's lastSeen time is older than the timeout period
        if (now - it->second.lastSeen > m_config.getServiceTimeoutMs()) {
            Log.printf(LOG_LEVEL_INFO, "[%s] Service timed out for node 0x%lx (Role: %d)\n", getName(), it->first, it->second.role);
            // Erase the element and update the iterator to the next valid element
            it = m_serviceTable.erase(it);
            removedCount++;
        } else {
            // If not timed out, move to the next element
            ++it;
        }
    }
    // Log if any entries were removed
    if (removedCount > 0) {
        Log.printf(LOG_LEVEL_DEBUG, "[%s] Removed %d timed-out service(s).\n", getName(), removedCount);
    }
}

/**
 * @brief Finds the "best" known gateway node from the service table.
 * Current Strategy: Returns the Node ID of the most recently seen gateway.
 * @return Node ID of the best gateway, or 0 if none are known/active.
 */
uint32_t AkitaSmartCityServices::findGatewayNode() {
    uint32_t bestGateway = 0;
    unsigned long latestSeen = 0;

    // Iterate through the service table
    for (const auto& pair : m_serviceTable) {
        // Check if the node's role is Gateway
        if (pair.second.role == ServiceDiscovery_Role_GATEWAY) {
            // If this gateway was seen more recently than the current best, update
            if (pair.second.lastSeen > latestSeen) {
                latestSeen = pair.second.lastSeen;
                bestGateway = pair.first; // pair.first is the Node ID (the map key)
            }
            // Future Enhancement: Could add more complex logic here, e.g.,
            // - Prefer gateways with a specific service ID.
            // - Check NodeDB for signal quality (RSSI/SNR) and prefer stronger signals.
            // - Implement round-robin or load balancing if multiple gateways are available.
        }
    }
    // Return the Node ID of the best gateway found, or 0 if none were found/active
    return bestGateway;
}


// --- MQTT Publishing & Buffering (Gateway Role) ---
#ifdef ASCS_ROLE_GATEWAY

/**
 * @brief Decides whether to publish a received packet directly via MQTT or buffer it.
 * Buffering occurs if MQTT is disconnected or if actively processing the buffer.
 * @param packet The received SmartCityPacket (must contain SensorData).
 * @param fromNode The originating Node ID of the packet.
 */
void AkitaSmartCityServices::publishMqttOrBuffer(const SmartCityPacket &packet, uint32_t fromNode) {
    // Ensure the MQTT client is initialized
    if (!m_mqttClient) {
        Log.println(LOG_LEVEL_ERROR, "[%s] MQTT client not initialized! Cannot publish or buffer.", getName());
        return;
    }
    // Ensure the packet contains sensor data
    if (packet.which_payload != SmartCityPacket_sensor_data_tag) {
         Log.println(LOG_LEVEL_WARNING, "[%s] Attempted to publish/buffer non-SensorData packet.", getName());
         return;
    }

    // Check MQTT connection status and buffering flag
    if (m_mqttClient->connected() && !m_gatewayBufferActive) {
        // --- Attempt Direct Publish ---
        Log.println(LOG_LEVEL_DEBUG, "[%s] MQTT connected. Attempting direct publish...", getName());
        // ** Need access to the decoded map data here **
        // This requires refactoring how decoded map data is passed down.
        // Placeholder: Call publishMqtt with only the base SensorData struct.
        // This will result in the map data being missing in the JSON payload.
        if (!publishMqtt(packet.payload.sensor_data, fromNode)) {
            // Direct publish failed (e.g., MQTT buffer full, network issue despite connection)
            Log.println(LOG_LEVEL_WARNING, "[%s] Direct MQTT publish failed! Activating buffering.", getName());
            m_gatewayBufferActive = true; // Start buffering subsequent messages
            bufferPacket(packet); // Buffer the current failed packet
        } else {
            // Direct publish successful
            Log.println(LOG_LEVEL_DEBUG, "[%s] Direct MQTT publish successful.", getName());
        }
    } else {
        // --- Buffer Packet ---
        // Reason: MQTT not connected OR currently processing buffer (avoids interleaved sends)
        if (!m_gatewayBufferActive) {
            // Log the start of buffering only once when the state changes
            Log.println(LOG_LEVEL_INFO, "[%s] MQTT disconnected or buffer active. Buffering packet.", getName());
            m_gatewayBufferActive = true; // Ensure flag is set
        } else {
             Log.println(LOG_LEVEL_DEBUG, "[%s] Buffering packet (MQTT disconnected or buffer active).", getName());
        }
        bufferPacket(packet); // Add the packet to the buffer file
    }
}


/**
 * @brief Performs the actual MQTT publication of sensor data.
 * Constructs the topic and JSON payload.
 * @param sensorData The SensorData struct to publish.
 * @param fromNode The originating Node ID.
 * @return True if the message was successfully published by the MQTT client, false otherwise.
 */
bool AkitaSmartCityServices::publishMqtt(const SensorData &sensorData, uint32_t fromNode) {
    // Double-check connection (should be called by publishMqttOrBuffer which already checks)
    if (!m_mqttClient || !m_mqttClient->connected()) {
        Log.println(LOG_LEVEL_WARNING, "[%s] publishMqtt called but client not connected.", getName());
        return false;
    }

    // --- Construct MQTT Topic ---
    char fromNodeHex[9]; // 8 hex chars + null terminator
    snprintf(fromNodeHex, sizeof(fromNodeHex), "%08lx", fromNode); // Format Node ID as hex string

    std::string topic = m_config.getMqttBaseTopic();
    topic += "/sensor/"; // Assume data originates from a sensor conceptually
    topic += std::to_string(m_config.getServiceId()); // Use Gateway's service ID for topic structure
    topic += "/";
    topic += fromNodeHex; // Add originating node ID
    // Add sensor ID from the packet if available
    if (strlen(sensorData.sensor_id) > 0) {
        topic += "/";
        topic += sensorData.sensor_id;
    }

    // --- Construct JSON Payload ---
    // Estimate JSON size needed. Adjust capacity as needed based on typical map size.
    // Base fields + map object overhead + estimated size per map entry + safety buffer
    const int base_size = JSON_OBJECT_SIZE(5); // node_id, sensor_id, timestamp, sequence, readings_obj
    const int estimated_entry_size = 35;       // Avg key len + value representation + quotes, colon, comma
    const int map_capacity = JSON_OBJECT_SIZE(sensorData.readings_count); // Map object overhead
    const int jsonCapacity = base_size + map_capacity + (sensorData.readings_count * estimated_entry_size) + 150; // Add safety buffer
    StaticJsonDocument<jsonCapacity> doc; // Increase size if publish fails due to truncation!

    // Populate base fields
    doc["node_id"] = fromNodeHex;
    doc["sensor_id"] = sensorData.sensor_id; // Can be empty
    doc["timestamp_utc"] = sensorData.timestamp_utc;
    doc["sequence_num"] = sensorData.sequence_num;

    // Add Meshtastic metadata (optional, requires NodeDB lookup)
    // NodeInfo *ni = m_api->getNode(fromNode);
    // if (ni) {
    //    JsonObject meta = doc.createNestedObject("meshtastic_meta");
    //    meta["rssi"] = ni->radio.rssi;
    //    meta["snr"] = ni->radio.snr;
    //    meta["hops_away"] = ni->hops_away;
    // }

    // Create nested object for readings
    JsonObject readingsObj = doc.createNestedObject("readings");

    // --- Populate Readings Object ---
    // ** CRITICAL DEPENDENCY: Access to the decoded map data **
    // This requires the decoded map (e.g., 'decoded_readings' from handleReceived)
    // to be passed down to this function. Without it, the map data cannot be added.
    // Placeholder logic assumes map data is unavailable here due to current structure.
    if (sensorData.readings_count > 0) {
        // If we had access to `const std::map<std::string, float>& readingsMap`:
        // for (const auto& pair : readingsMap) {
        //     readingsObj[pair.first] = pair.second;
        // }
        // Without access, add a status note:
        readingsObj["status"] = "Map data processing requires refactoring to pass decoded map here.";
        Log.println(LOG_LEVEL_WARNING, "[%s] Cannot add map readings to JSON: Decoded map data not available in publishMqtt context.", getName());
    } else {
        readingsObj["status"] = "Map data unavailable or empty in source packet.";
    }
    // --- End Populate Readings ---


    // Serialize JSON document to string
    std::string payload;
    size_t json_len = serializeJson(doc, payload);

    // Check for serialization errors (e.g., buffer too small)
    if (json_len == 0) {
        Log.println(LOG_LEVEL_ERROR, "[%s] JSON serialization failed (payload empty)! Increase StaticJsonDocument capacity?", getName());
        return false;
    }
     if (doc.overflowed()) {
         Log.println(LOG_LEVEL_WARNING, "[%s] StaticJsonDocument overflowed during serialization. Payload truncated.", getName());
         // Publish might still succeed but with incomplete data. Consider returning false.
     }


    Log.printf(LOG_LEVEL_INFO, "[%s] Publishing to MQTT topic: %s\n", getName(), topic.c_str());
    Log.printf(LOG_LEVEL_DEBUG, "[%s] MQTT Payload (%d bytes): %s\n", getName(), json_len, payload.c_str());

    // --- Publish to MQTT ---
    // feed_watchdog_placeholder(); // Feed before potentially blocking network operation
    bool success = m_mqttClient->publish(topic.c_str(), payload.c_str(), false); // Retain=false for sensor data
    // feed_watchdog_placeholder(); // Feed after potentially blocking network operation

    if (!success) {
        // Publish failed. Could be due to buffer size in PubSubClient, network issue, etc.
        Log.println(LOG_LEVEL_ERROR, "[%s] MQTT publish failed! Check PubSubClient buffer size and connection state.", getName());
    }
    return success;
}


/**
 * @brief Appends an encoded SmartCityPacket to the buffer file on the filesystem.
 * Uses simple framing: [uint16_t length][packet_bytes].
 * @param packet The SmartCityPacket to buffer (assumes map callbacks are set if needed).
 */
void AkitaSmartCityServices::bufferPacket(const SmartCityPacket &packet) {
    Log.println(LOG_LEVEL_INFO, "[%s] Buffering packet...", getName());

    // Encode the packet into a temporary buffer
    uint8_t buffer[ASCS_GATEWAY_MAX_PACKET_SIZE];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    // --- Encoding preparation for map (if sensor data) ---
    // This part is tricky when buffering. We need the original map data to re-encode.
    // If the packet originated from the mesh, its map field callbacks might be set,
    // but the associated 'arg' (pointing to the original map) might be invalid now.
    // Safest Approach: The caller (publishMqttOrBuffer) should ideally pass the
    // original std::map along if buffering might occur.
    // Current Simplification: Assume the packet passed in might have valid callbacks/arg
    // if it came directly from runSensorLogic (unlikely for gateway buffer).
    // If it came from the mesh, the map data will likely NOT be encoded correctly here.
    // --> This needs refinement based on how map data is managed through the call stack. <--

    if (!pb_encode(&stream, SmartCityPacket_fields, &packet)) {
        Log.printf(LOG_LEVEL_ERROR, "[%s] Failed to encode packet for buffering: %s\n", getName(), PB_GET_ERROR(&stream));
        return; // Cannot buffer if encoding fails
    }

    size_t len = stream.bytes_written;
    // Validate encoded length
    if (len == 0 || len > ASCS_GATEWAY_MAX_PACKET_SIZE) {
        Log.printf(LOG_LEVEL_ERROR, "[%s] Invalid encoded packet size (%d) for buffering.\n", getName(), len);
        return;
    }

    // Open buffer file in append mode
    File file = FileSystem.open(ASCS_GATEWAY_BUFFER_FILENAME, FILE_APPEND);
    if (!file) {
        Log.println(LOG_LEVEL_ERROR, "[%s] Failed to open buffer file for append!", getName());
        return; // Cannot buffer if file cannot be opened
    }

    // Check if adding this packet exceeds the max buffer size
    // Add size of length prefix + packet length
    if (file.size() + sizeof(uint16_t) + len > ASCS_GATEWAY_BUFFER_MAX_SIZE) {
        Log.println(LOG_LEVEL_WARNING, "[%s] Buffer file full (or would exceed limit). Packet dropped.", getName());
        // --- TODO: Implement Buffer Management ---
        // Options:
        // 1. Drop Oldest: Read and discard packets from the beginning until space is sufficient. (Complex file I/O)
        // 2. Drop Current: Simply don't write the new packet (as done here).
        // 3. Circular Buffer File: More complex but efficient.
        file.close();
        return;
    }

    // Write length prefix (uint16_t)
    uint16_t msg_len = (uint16_t)len;
    size_t written = file.write((uint8_t*)&msg_len, sizeof(uint16_t));
    if (written != sizeof(uint16_t)) {
         Log.println(LOG_LEVEL_ERROR, "[%s] Failed to write length prefix to buffer file!", getName());
         file.close();
         return;
    }

    // Write actual packet data
    written = file.write(buffer, len);
    file.close(); // Close file immediately after writing

    if (written == len) {
        Log.printf(LOG_LEVEL_INFO, "[%s] Packet buffered (%d bytes).\n", getName(), len);
    } else {
        // This indicates a potentially serious filesystem issue
        Log.printf(LOG_LEVEL_ERROR, "[%s] Failed to write full packet data to buffer file! Wrote %d/%d bytes.\n", getName(), written, len);
        // Consider attempting to truncate the file to remove partial write?
    }
}

/**
 * @brief Reads the next length-prefixed packet from the beginning of the buffer file.
 * @param file An open File handle for the buffer file (read mode).
 * @param buffer Buffer to store the packet data.
 * @param len Output parameter: Stores the length of the packet read.
 * @return True if a packet was successfully read, false otherwise (EOF, error, corruption).
 */
bool AkitaSmartCityServices::readPacketFromBuffer(File &file, uint8_t* buffer, size_t &len) {
    // Ensure file is valid and has enough data for at least the length prefix
    if (!file || file.available() < sizeof(uint16_t)) {
        return false;
    }

    // Read the 16-bit length prefix
    uint16_t msg_len;
    if (file.read((uint8_t*)&msg_len, sizeof(uint16_t)) != sizeof(uint16_t)) {
        Log.println(LOG_LEVEL_ERROR, "[%s] Failed to read length prefix from buffer.", getName());
        return false; // Read error
    }

    // Validate the read length
    if (msg_len == 0 || msg_len > ASCS_GATEWAY_MAX_PACKET_SIZE) {
        Log.printf(LOG_LEVEL_ERROR, "[%s] Invalid packet length (%d) read from buffer. Buffer possibly corrupted.\n", getName(), msg_len);
        // --- TODO: Handle Buffer Corruption ---
        // Options: Skip ahead, clear file, attempt recovery. For now, return error.
        return false;
    }

    // Check if enough data remains in the file for the packet itself
    if (file.available() < msg_len) {
        Log.printf(LOG_LEVEL_ERROR, "[%s] Buffer file truncated? Expected %d bytes for packet, have %d remaining.\n", getName(), msg_len, file.available());
        return false; // Not enough data
    }

    // Read the packet data into the provided buffer
    if (file.read(buffer, msg_len) != msg_len) {
        Log.println(LOG_LEVEL_ERROR, "[%s] Failed to read packet data from buffer.", getName());
        return false; // Read error
    }

    len = msg_len; // Set the output length parameter
    return true; // Packet successfully read
}

/**
 * @brief Removes the first packet from the buffer file.
 * This is a basic implementation that copies the remaining data to a temporary file
 * and then replaces the original file. This can be slow for large buffers.
 */
void AkitaSmartCityServices::removePacketFromBuffer() {
    Log.println(LOG_LEVEL_DEBUG, "[%s] Removing processed packet from buffer file...", getName());
    // feed_watchdog_placeholder(); // Feed watchdog before potentially long file I/O

    // Open the buffer file for reading
    File readFile = FileSystem.open(ASCS_GATEWAY_BUFFER_FILENAME, FILE_READ);
    if (!readFile) {
        Log.println(LOG_LEVEL_ERROR, "[%s] Failed to open buffer for reading (removePacket).", getName());
        return; // Cannot proceed
    }

    // Read the length of the first packet to know how much to skip
    uint16_t msg_len;
    if (readFile.read((uint8_t*)&msg_len, sizeof(uint16_t)) != sizeof(uint16_t)) {
        Log.println(LOG_LEVEL_ERROR, "[%s] Failed to read length prefix for removal. Buffer might be empty or corrupted.", getName());
        readFile.close();
        // If buffer was likely just empty, maybe try deleting it?
        if (readFile.size() < sizeof(uint16_t)) {
             FileSystem.remove(ASCS_GATEWAY_BUFFER_FILENAME);
        }
        return;
    }

    // Validate length again
     if (msg_len == 0 || msg_len > ASCS_GATEWAY_MAX_PACKET_SIZE) {
          Log.printf(LOG_LEVEL_ERROR, "[%s] Invalid packet length (%d) found during removal. Aborting removal.\n", getName(), msg_len);
          readFile.close();
          // Consider clearing the buffer file if corruption is suspected.
          // FileSystem.remove(ASCS_GATEWAY_BUFFER_FILENAME);
          return;
     }


    // Calculate the starting position of the *next* packet
    size_t nextPacketPos = sizeof(uint16_t) + msg_len;
    size_t totalSize = readFile.size();

    // If the position of the next packet is at or beyond the file size,
    // it means we just removed the last (or only) packet.
    if (nextPacketPos >= totalSize) {
        readFile.close(); // Close the read handle
        // Delete the buffer file as it's now empty
        if (!FileSystem.remove(ASCS_GATEWAY_BUFFER_FILENAME)) {
            Log.println(LOG_LEVEL_ERROR, "[%s] Failed to remove empty buffer file.", getName());
        } else {
            Log.println(LOG_LEVEL_DEBUG, "[%s] Buffer file empty after removal, deleted.", getName());
        }
        return; // Done
    }

    // --- Copy remaining data to a temporary file ---
    const char* tempFilename = "/ascs_buffer.tmp";
    File writeFile = FileSystem.open(tempFilename, FILE_WRITE); // Open temp file for writing
    if (!writeFile) {
        Log.println(LOG_LEVEL_ERROR, "[%s] Failed to open temp buffer file for writing!", getName());
        readFile.close(); // Close the read handle
        return; // Cannot proceed
    }

    // Seek the read file handle past the packet we are removing
    readFile.seek(nextPacketPos, SeekSet);

    // Buffer for copying data
    uint8_t copyBuf[128];
    size_t bytesCopied = 0;
    // Read from original file and write to temp file
    while (readFile.available()) {
        // feed_watchdog_placeholder(); // Feed during potentially long copy loop
        size_t bytesRead = readFile.read(copyBuf, sizeof(copyBuf));
        if (bytesRead > 0) {
            size_t bytesWritten = writeFile.write(copyBuf, bytesRead);
            if (bytesWritten != bytesRead) {
                 Log.println(LOG_LEVEL_ERROR, "[%s] Failed to write data to temp buffer file!", getName());
                 // Abort the process, cleanup might be needed
                 readFile.close();
                 writeFile.close();
                 FileSystem.remove(tempFilename); // Remove incomplete temp file
                 return;
            }
            bytesCopied += bytesWritten;
        } else {
            // End of file or read error
            break;
        }
    }

    // Close both files
    readFile.close();
    writeFile.close();
    Log.printf(LOG_LEVEL_DEBUG, "[%s] Copied %d bytes to temporary buffer file.\n", getName(), bytesCopied);

    // --- Replace original buffer file with the temporary file ---
    if (!FileSystem.remove(ASCS_GATEWAY_BUFFER_FILENAME)) {
        Log.println(LOG_LEVEL_ERROR, "[%s] Failed to remove original buffer file during replace.", getName());
        // Attempt to remove the temp file as well to avoid leaving it orphaned
        FileSystem.remove(tempFilename);
    } else {
        // Original removed, now rename temp file to the original name
        if (!FileSystem.rename(tempFilename, ASCS_GATEWAY_BUFFER_FILENAME)) {
            Log.println(LOG_LEVEL_ERROR, "[%s] Failed to rename temp buffer file to original name!", getName());
            // This is problematic - buffer might be lost or corrupted
        } else {
            Log.println(LOG_LEVEL_DEBUG, "[%s] Buffer file updated successfully after packet removal.", getName());
        }
    }
     // feed_watchdog_placeholder(); // Feed watchdog after potentially long file I/O
}


/**
 * @brief Attempts to read, decode, publish, and remove one packet from the buffer file.
 * Called periodically when MQTT is connected.
 */
void AkitaSmartCityServices::processBufferedPackets() {
    // Only process if MQTT is connected and client is initialized
    if (!m_mqttClient || !m_mqttClient->connected()) {
        return;
    }

    // Open buffer file for reading
    File file = FileSystem.open(ASCS_GATEWAY_BUFFER_FILENAME, FILE_READ);
    // Check if file exists and is not empty
    if (!file || file.size() == 0) {
        if (file) file.close(); // Close if opened but empty
        // Buffer is empty, ensure buffering flag is off
        if (m_gatewayBufferActive) {
             Log.println(LOG_LEVEL_INFO, "[%s] Buffer is empty, stopping buffer processing.", getName());
             m_gatewayBufferActive = false;
        }
        return; // Nothing to process
    }

    // Log only once when starting to process a non-empty buffer
    if (!m_gatewayBufferActive) {
        Log.println(LOG_LEVEL_INFO, "[%s] Processing buffered packets...", getName());
        m_gatewayBufferActive = true; // Set flag to indicate buffer processing is active
    }

    // Buffer for reading packet data
    uint8_t buffer[ASCS_GATEWAY_MAX_PACKET_SIZE];
    size_t len;

    // --- Read the first packet from the file ---
    if (readPacketFromBuffer(file, buffer, len)) {
        file.close(); // Close the read handle BEFORE attempting to publish/remove

        // --- Decode the packet ---
        SmartCityPacket scp = SmartCityPacket_init_zero;
        pb_istream_t stream = pb_istream_from_buffer(buffer, len);

        // Prepare context for decoding map fields
        MapCallbackContext decode_context;
        std::map<std::string, float> decoded_readings; // Temporary map for decoded readings
        decode_context.map_ptr = &decoded_readings;
        scp.payload.sensor_data.readings.funcs.decode = decode_map_callback;
        scp.payload.sensor_data.readings.arg = &decode_context;

        if (pb_decode(&stream, SmartCityPacket_fields, &scp)) {
            // --- Packet Decoded Successfully ---
            if (scp.which_payload == SmartCityPacket_sensor_data_tag) {
                // --- TODO: Retrieve original 'fromNode' ---
                // The current buffer format ONLY stores the packet bytes.
                // The originating node ID is lost.
                // Required Changes:
                // 1. bufferPacket: Write `uint32_t fromNode` *before* the length/packet.
                // 2. readPacketFromBuffer: Read `fromNode` *before* reading length/packet.
                // 3. removePacketFromBuffer: Account for the extra 4 bytes of `fromNode` when calculating offsets/copying.

                // Placeholder: Use 0 as fromNode until buffer format is fixed
                uint32_t fromNode = 0; // <--- FIX REQUIRED
                Log.println(LOG_LEVEL_WARNING, "[%s] Cannot determine originating node for buffered packet! Using 0.", getName());

                // --- Attempt to publish the decoded packet ---
                // Pass the decoded SensorData struct. Map data access still needs refactoring in publishMqtt.
                if (publishMqtt(scp.payload.sensor_data, fromNode)) {
                    // --- Publish Successful: Remove from buffer ---
                    Log.println(LOG_LEVEL_DEBUG, "[%s] Successfully published buffered packet.", getName());
                    removePacketFromBuffer();
                    // Immediately try to process the next packet in the buffer if MQTT is still connected
                    // Reschedule the buffer check very soon by adjusting the timer.
                    m_lastBufferProcessTime = millis() - m_config.getMqttReconnectIntervalMs(); // Check again quickly
                } else {
                    // Publish failed even though MQTT *was* connected.
                    // Could be temporary issue, MQTT buffer size, etc.
                    Log.println(LOG_LEVEL_WARNING, "[%s] Failed to publish buffered packet. MQTT issue? Stopping buffer processing for now.", getName());
                    // Stop processing buffer for this cycle to avoid hammering a potentially failing connection.
                    // The packet remains at the front of the buffer. Will retry on next check interval.
                    m_gatewayBufferActive = false; // Allow direct publish attempts again if connection recovers
                }
            } else {
                // Packet in buffer is not SensorData (shouldn't normally happen)
                Log.println(LOG_LEVEL_WARNING, "[%s] Buffered packet is not SensorData. Discarding.", getName());
                removePacketFromBuffer(); // Remove unexpected packet type
            }
        } else {
            // --- Decoding Failed ---
            Log.printf(LOG_LEVEL_ERROR, "[%s] Failed to decode buffered packet: %s. Discarding corrupted data.\n", getName(), PB_GET_ERROR(&stream));
            removePacketFromBuffer(); // Remove the corrupted packet
        }
    } else {
        // --- Reading Failed ---
        file.close(); // Close the file handle
        Log.println(LOG_LEVEL_ERROR, "[%s] Failed to read packet from buffer file. File might be corrupted or empty.", getName());
        // Consider clearing the buffer file if reading consistently fails?
        // FileSystem.remove(ASCS_GATEWAY_BUFFER_FILENAME);
        m_gatewayBufferActive = false; // Stop trying if read fails
    }

    // Final check: If buffer processing was active but file is now empty, clear the flag
    if (m_gatewayBufferActive) {
        file = FileSystem.open(ASCS_GATEWAY_BUFFER_FILENAME, FILE_READ);
        if (!file || file.size() == 0) {
            if (file) file.close();
             Log.println(LOG_LEVEL_INFO, "[%s] Buffer processing complete (buffer empty).", getName());
             m_gatewayBufferActive = false;
        } else {
            if (file) file.close();
            // Buffer still has data, flag remains true, will process next time
        }
    }
}

#else
// Provide empty stubs for Gateway buffering functions if support is not compiled in.
void AkitaSmartCityServices::publishMqttOrBuffer(const SmartCityPacket &, uint32_t) {}
bool AkitaSmartCityServices::publishMqtt(const SensorData &, uint32_t) { return false; }
void AkitaSmartCityServices::bufferPacket(const SmartCityPacket &) {}
void AkitaSmartCityServices::processBufferedPackets() {}
bool AkitaSmartCityServices::readPacketFromBuffer(File &, uint8_t*, size_t &) { return false; }
void AkitaSmartCityServices::removePacketFromBuffer() {}
#endif // ASCS_ROLE_GATEWAY


