#ifndef ASCS_CONFIG_H
#define ASCS_CONFIG_H

#include <Preferences.h>
#include <string>
#include "generated_proto/SmartCity.pb.h" // For ServiceDiscovery_Role enum

// --- Default Configuration Constants ---
// Defined here for clarity, but could be in a separate config_defaults.h

#define ASCS_DEFAULT_ROLE ServiceDiscovery_Role_SENSOR
#define ASCS_DEFAULT_SERVICE_ID 1
#define ASCS_DEFAULT_TARGET_NODE 0 // 0 means auto-discover/broadcast
#define ASCS_DEFAULT_SENSOR_READ_INTERVAL_MS 60000
#define ASCS_DEFAULT_DISCOVERY_INTERVAL_MS 300000
#define ASCS_DEFAULT_SERVICE_TIMEOUT_MS 900000 // 3x discovery interval
#define ASCS_DEFAULT_MQTT_RECONNECT_INTERVAL_MS 10000

#define ASCS_DEFAULT_WIFI_SSID "YourWiFi_SSID"
#define ASCS_DEFAULT_WIFI_PASSWORD "YourWiFiPassword"
#define ASCS_DEFAULT_MQTT_SERVER "your_mqtt_broker.com"
#define ASCS_DEFAULT_MQTT_PORT 1883
#define ASCS_DEFAULT_MQTT_USER ""
#define ASCS_DEFAULT_MQTT_PASSWORD ""
#define ASCS_DEFAULT_MQTT_BASE_TOPIC "akita/smartcity"

#define ASCS_PREFERENCES_NAMESPACE "ascs"

class ASCSConfig {
public:
    ASCSConfig();
    ~ASCSConfig();

    // Load configuration from Preferences
    void load();
    // Save configuration (optional, if needed for runtime changes)
    // void save();

    // --- Getters for configuration values ---
    ServiceDiscovery_Role getNodeRole() const;
    uint32_t getServiceId() const;
    uint32_t getTargetNodeId() const;
    uint32_t getSensorReadIntervalMs() const;
    uint32_t getDiscoveryIntervalMs() const;
    uint32_t getServiceTimeoutMs() const;
    uint32_t getMqttReconnectIntervalMs() const; // Added getter

    // Gateway specific getters
    std::string getWifiSsid() const;
    std::string getWifiPassword() const;
    std::string getMqttServer() const;
    int getMqttPort() const;
    std::string getMqttUser() const;
    std::string getMqttPassword() const;
    std::string getMqttBaseTopic() const;

private:
    Preferences m_preferences;

    // Cached configuration values
    ServiceDiscovery_Role m_nodeRole;
    uint32_t m_serviceId;
    uint32_t m_targetNodeId;
    uint32_t m_sensorReadIntervalMs;
    uint32_t m_discoveryIntervalMs;
    uint32_t m_serviceTimeoutMs;
    uint32_t m_mqttReconnectIntervalMs;

    // Gateway specific
    std::string m_wifiSsid;
    std::string m_wifiPassword;
    std::string m_mqttServer;
    int m_mqttPort;
    std::string m_mqttUser;
    std::string m_mqttPassword;
    std::string m_mqttBaseTopic;
};

#endif // ASCS_CONFIG_H
