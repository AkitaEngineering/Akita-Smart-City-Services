#include "ASCSConfig.h"
#include "plugin_api.h" // For Log definition

ASCSConfig::ASCSConfig() {
    // Constructor: Preferences object is initialized in the header
}

ASCSConfig::~ASCSConfig() {
    m_preferences.end(); // Close Preferences
}

void ASCSConfig::load() {
    Log.println(LOG_LEVEL_DEBUG, "ASCSConfig: Loading configuration...");
    if (!m_preferences.begin(ASCS_PREFERENCES_NAMESPACE, false)) { // read/write false initially
         Log.println(LOG_LEVEL_ERROR, "ASCSConfig: Failed to initialize Preferences!");
         // Use defaults if Preferences fails
         m_nodeRole = ASCS_DEFAULT_ROLE;
         m_serviceId = ASCS_DEFAULT_SERVICE_ID;
         m_targetNodeId = ASCS_DEFAULT_TARGET_NODE;
         m_sensorReadIntervalMs = ASCS_DEFAULT_SENSOR_READ_INTERVAL_MS;
         m_discoveryIntervalMs = ASCS_DEFAULT_DISCOVERY_INTERVAL_MS;
         m_serviceTimeoutMs = ASCS_DEFAULT_SERVICE_TIMEOUT_MS;
         m_mqttReconnectIntervalMs = ASCS_DEFAULT_MQTT_RECONNECT_INTERVAL_MS;
         m_wifiSsid = ASCS_DEFAULT_WIFI_SSID;
         m_wifiPassword = ASCS_DEFAULT_WIFI_PASSWORD;
         m_mqttServer = ASCS_DEFAULT_MQTT_SERVER;
         m_mqttPort = ASCS_DEFAULT_MQTT_PORT;
         m_mqttUser = ASCS_DEFAULT_MQTT_USER;
         m_mqttPassword = ASCS_DEFAULT_MQTT_PASSWORD;
         m_mqttBaseTopic = ASCS_DEFAULT_MQTT_BASE_TOPIC;
         return;
    }

    m_nodeRole = (ServiceDiscovery_Role)m_preferences.getUInt("role", ASCS_DEFAULT_ROLE);
    m_serviceId = m_preferences.getUInt("service_id", ASCS_DEFAULT_SERVICE_ID);
    m_targetNodeId = m_preferences.getUInt("target_node", ASCS_DEFAULT_TARGET_NODE);
    m_sensorReadIntervalMs = m_preferences.getUInt("read_int", ASCS_DEFAULT_SENSOR_READ_INTERVAL_MS);
    m_discoveryIntervalMs = m_preferences.getUInt("disc_int", ASCS_DEFAULT_DISCOVERY_INTERVAL_MS);
    m_serviceTimeoutMs = m_preferences.getUInt("svc_tout", ASCS_DEFAULT_SERVICE_TIMEOUT_MS);
    // Load new interval, defaulting if not present
    m_mqttReconnectIntervalMs = m_preferences.getUInt("mqtt_rec_int", ASCS_DEFAULT_MQTT_RECONNECT_INTERVAL_MS);


    // Load gateway settings only if the role *might* be gateway, avoids unnecessary string ops
    // Note: The plugin logic still needs the #ifdef ASCS_ROLE_GATEWAY for compilation
    if (m_nodeRole == ServiceDiscovery_Role_GATEWAY || ASCS_DEFAULT_ROLE == ServiceDiscovery_Role_GATEWAY) {
         m_wifiSsid = m_preferences.getString("wifi_ssid", ASCS_DEFAULT_WIFI_SSID).c_str();
         m_wifiPassword = m_preferences.getString("wifi_pass", ASCS_DEFAULT_WIFI_PASSWORD).c_str();
         m_mqttServer = m_preferences.getString("mqtt_srv", ASCS_DEFAULT_MQTT_SERVER).c_str();
         m_mqttPort = m_preferences.getInt("mqtt_port", ASCS_DEFAULT_MQTT_PORT);
         m_mqttUser = m_preferences.getString("mqtt_user", ASCS_DEFAULT_MQTT_USER).c_str();
         m_mqttPassword = m_preferences.getString("mqtt_pass", ASCS_DEFAULT_MQTT_PASSWORD).c_str();
         m_mqttBaseTopic = m_preferences.getString("mqtt_topic", ASCS_DEFAULT_MQTT_BASE_TOPIC).c_str();
    } else {
        // Ensure defaults are loaded if role is not gateway
         m_wifiSsid = ASCS_DEFAULT_WIFI_SSID;
         m_wifiPassword = ASCS_DEFAULT_WIFI_PASSWORD;
         m_mqttServer = ASCS_DEFAULT_MQTT_SERVER;
         m_mqttPort = ASCS_DEFAULT_MQTT_PORT;
         m_mqttUser = ASCS_DEFAULT_MQTT_USER;
         m_mqttPassword = ASCS_DEFAULT_MQTT_PASSWORD;
         m_mqttBaseTopic = ASCS_DEFAULT_MQTT_BASE_TOPIC;
    }

     Log.println(LOG_LEVEL_DEBUG, "ASCSConfig: Configuration loaded.");
     // Log loaded values if needed for debugging
     // Log.printf(LOG_LEVEL_DEBUG, "ASCSConfig: Role=%d, ServiceID=%lu, Target=0x%lx\n", m_nodeRole, m_serviceId, m_targetNodeId);
     // Log.printf(LOG_LEVEL_DEBUG, "ASCSConfig: Intervals: Read=%lu, Disc=%lu, Timeout=%lu, MQTTRec=%lu\n", m_sensorReadIntervalMs, m_discoveryIntervalMs, m_serviceTimeoutMs, m_mqttReconnectIntervalMs);
     // if (m_nodeRole == ServiceDiscovery_Role_GATEWAY) {
     //    Log.printf(LOG_LEVEL_DEBUG, "ASCSConfig: GW: WiFi=%s, MQTT=%s:%d\n", m_wifiSsid.c_str(), m_mqttServer.c_str(), m_mqttPort);
     // }
}

// void ASCSConfig::save() {
//     // Example: Open preferences in read/write mode and save a value
//     // if (!m_preferences.begin(ASCS_PREFERENCES_NAMESPACE, false)) { // false = read/write
//     //      Log.println(LOG_LEVEL_ERROR, "ASCSConfig: Failed to open Preferences for writing!");
//     //      return;
//     // }
//     // m_preferences.putUInt("role", m_nodeRole);
//     // m_preferences.end(); // Close after writing
//     // Log.println(LOG_LEVEL_INFO, "ASCSConfig: Configuration saved.");
// }


// --- Getters ---

ServiceDiscovery_Role ASCSConfig::getNodeRole() const { return m_nodeRole; }
uint32_t ASCSConfig::getServiceId() const { return m_serviceId; }
uint32_t ASCSConfig::getTargetNodeId() const { return m_targetNodeId; }
uint32_t ASCSConfig::getSensorReadIntervalMs() const { return m_sensorReadIntervalMs; }
uint32_t ASCSConfig::getDiscoveryIntervalMs() const { return m_discoveryIntervalMs; }
uint32_t ASCSConfig::getServiceTimeoutMs() const { return m_serviceTimeoutMs; }
uint32_t ASCSConfig::getMqttReconnectIntervalMs() const { return m_mqttReconnectIntervalMs; }


std::string ASCSConfig::getWifiSsid() const { return m_wifiSsid; }
std::string ASCSConfig::getWifiPassword() const { return m_wifiPassword; }
std::string ASCSConfig::getMqttServer() const { return m_mqttServer; }
int ASCSConfig::getMqttPort() const { return m_mqttPort; }
std::string ASCSConfig::getMqttUser() const { return m_mqttUser; }
std::string ASCSConfig::getMqttPassword() const { return m_mqttPassword; }
std::string ASCSConfig::getMqttBaseTopic() const { return m_mqttBaseTopic; }

