#include "ASCSConfig.h"
#include "DebugConfiguration.h"
#include <cctype>

namespace {
bool isValidMqttBaseTopic(const std::string &topic) {
    if (topic.empty() || topic.size() > 128 || topic.front() == '/' || topic.back() == '/') return false;
    bool previousSlash = false;
    for (const unsigned char character : topic) {
        if (character == '#' || character == '+' || std::iscntrl(character) || std::isspace(character)) return false;
        if (character == '/') {
            if (previousSlash) return false;
            previousSlash = true;
        } else {
            previousSlash = false;
        }
    }
    return true;
}

bool isValidMqttHost(const std::string &host) {
    if (host.empty() || host.size() > 253 || host.front() == '.' || host.back() == '.') return false;
    size_t labelLength = 0;
    bool previousHyphen = false;
    for (const unsigned char character : host) {
        if (character == '.') {
            if (labelLength == 0 || previousHyphen) return false;
            labelLength = 0;
            previousHyphen = false;
            continue;
        }
        if (!std::isalnum(character) && character != '-') return false;
        if ((labelLength == 0 && character == '-') || ++labelLength > 63) return false;
        previousHyphen = character == '-';
    }
    return labelLength > 0 && !previousHyphen;
}
}

ASCSConfig::ASCSConfig() = default;

ASCSConfig::~ASCSConfig() {
    m_preferences.end(); // Close Preferences
}

void ASCSConfig::load() {
    LOG_DEBUG("ASCSConfig: Loading configuration...");
    if (!m_preferences.begin(ASCS_PREFERENCES_NAMESPACE, true)) {
         LOG_ERROR("ASCSConfig: Failed to initialize Preferences!");
         m_nodeRole = ASCS_DEFAULT_ROLE;
         m_serviceId = ASCS_DEFAULT_SERVICE_ID;
         m_targetNodeId = ASCS_DEFAULT_TARGET_NODE;
         m_trustedGatewayNodeId = ASCS_DEFAULT_TRUSTED_GATEWAY_NODE;
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
         m_mqttCaCert = ASCS_DEFAULT_MQTT_CA_CERT;
         m_valid = false;
         m_validationError = "NVS preferences could not be opened";
         return;
    }

    const uint32_t schemaVersion = m_preferences.getUInt("schema_ver", 0);
    m_nodeRole = (ServiceDiscovery_Role)m_preferences.getUInt("role", ASCS_DEFAULT_ROLE);
    m_serviceId = m_preferences.getUInt("service_id", ASCS_DEFAULT_SERVICE_ID);
    m_targetNodeId = m_preferences.getUInt("target_node", ASCS_DEFAULT_TARGET_NODE);
    m_trustedGatewayNodeId = m_preferences.getUInt("trusted_gw", ASCS_DEFAULT_TRUSTED_GATEWAY_NODE);
    m_sensorReadIntervalMs = m_preferences.getUInt("read_int", ASCS_DEFAULT_SENSOR_READ_INTERVAL_MS);
    m_discoveryIntervalMs = m_preferences.getUInt("disc_int", ASCS_DEFAULT_DISCOVERY_INTERVAL_MS);
    m_serviceTimeoutMs = m_preferences.getUInt("svc_tout", ASCS_DEFAULT_SERVICE_TIMEOUT_MS);
    m_mqttReconnectIntervalMs = m_preferences.getUInt("mqtt_rec_int", ASCS_DEFAULT_MQTT_RECONNECT_INTERVAL_MS);


    if (m_nodeRole == ServiceDiscovery_Role_GATEWAY || ASCS_DEFAULT_ROLE == ServiceDiscovery_Role_GATEWAY) {
         m_wifiSsid = m_preferences.getString("wifi_ssid", ASCS_DEFAULT_WIFI_SSID).c_str();
         m_wifiPassword = m_preferences.getString("wifi_pass", ASCS_DEFAULT_WIFI_PASSWORD).c_str();
         m_mqttServer = m_preferences.getString("mqtt_srv", ASCS_DEFAULT_MQTT_SERVER).c_str();
         m_mqttPort = static_cast<int>(m_preferences.getUInt("mqtt_port", ASCS_DEFAULT_MQTT_PORT));
         m_mqttUser = m_preferences.getString("mqtt_user", ASCS_DEFAULT_MQTT_USER).c_str();
         m_mqttPassword = m_preferences.getString("mqtt_pass", ASCS_DEFAULT_MQTT_PASSWORD).c_str();
         m_mqttBaseTopic = m_preferences.getString("mqtt_topic", ASCS_DEFAULT_MQTT_BASE_TOPIC).c_str();
         m_mqttCaCert = m_preferences.getString("mqtt_ca", ASCS_DEFAULT_MQTT_CA_CERT).c_str();
    } else {
         m_wifiSsid = ASCS_DEFAULT_WIFI_SSID;
         m_wifiPassword = ASCS_DEFAULT_WIFI_PASSWORD;
         m_mqttServer = ASCS_DEFAULT_MQTT_SERVER;
         m_mqttPort = ASCS_DEFAULT_MQTT_PORT;
         m_mqttUser = ASCS_DEFAULT_MQTT_USER;
         m_mqttPassword = ASCS_DEFAULT_MQTT_PASSWORD;
         m_mqttBaseTopic = ASCS_DEFAULT_MQTT_BASE_TOPIC;
         m_mqttCaCert = ASCS_DEFAULT_MQTT_CA_CERT;
    }

    m_valid = true;
    m_validationError.clear();
    if (schemaVersion != ASCS_CONFIG_SCHEMA_VERSION) {
        m_valid = false; m_validationError = "configuration is missing or uses an unsupported schema version; reprovision this device";
    } else if (m_nodeRole < ServiceDiscovery_Role_SENSOR || m_nodeRole > ServiceDiscovery_Role_GATEWAY) {
        m_valid = false; m_validationError = "role must be SENSOR, AGGREGATOR, or GATEWAY";
    } else if (m_nodeRole != static_cast<ServiceDiscovery_Role>(ASCS_DEFAULT_ROLE)) {
        m_valid = false; m_validationError = "provisioned role does not match this firmware image";
    } else if (m_serviceId == 0) {
        m_valid = false; m_validationError = "service_id must be non-zero";
    } else if (m_sensorReadIntervalMs < 1000 || m_discoveryIntervalMs < 1000) {
        m_valid = false; m_validationError = "read and discovery intervals must be at least 1000 ms";
    } else if (m_serviceTimeoutMs <= m_discoveryIntervalMs) {
        m_valid = false; m_validationError = "service timeout must exceed discovery interval";
    } else if (m_mqttReconnectIntervalMs < 1000) {
        m_valid = false; m_validationError = "MQTT reconnect interval must be at least 1000 ms";
    } else if (m_nodeRole == ServiceDiscovery_Role_GATEWAY &&
               (m_wifiSsid.empty() || m_mqttServer.empty() || m_mqttUser.empty() || m_mqttPassword.empty() ||
                m_mqttCaCert.empty() || m_mqttPort < 1 || m_mqttPort > 65535)) {
        m_valid = false;
        m_validationError = "gateway requires WiFi, authenticated MQTT, a valid port, and mqtt_ca TLS certificate";
    } else if (m_nodeRole == ServiceDiscovery_Role_GATEWAY &&
               (m_wifiSsid.size() > 32 || m_wifiPassword.size() > 64 || !isValidMqttHost(m_mqttServer) ||
                m_mqttUser.size() > 256 || m_mqttPassword.size() > 4096 || m_mqttCaCert.size() > 8192 ||
                !isValidMqttBaseTopic(m_mqttBaseTopic))) {
        m_valid = false;
        m_validationError = "gateway WiFi or MQTT configuration exceeds a safe limit or contains invalid characters";
    }

     LOG_DEBUG("ASCSConfig: Configuration loaded.");
}


// --- Getters ---

ServiceDiscovery_Role ASCSConfig::getNodeRole() const { return m_nodeRole; }
bool ASCSConfig::isValid() const { return m_valid; }
std::string ASCSConfig::getValidationError() const { return m_validationError; }
uint32_t ASCSConfig::getServiceId() const { return m_serviceId; }
uint32_t ASCSConfig::getTargetNodeId() const { return m_targetNodeId; }
uint32_t ASCSConfig::getTrustedGatewayNodeId() const { return m_trustedGatewayNodeId; }
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
std::string ASCSConfig::getMqttCaCert() const { return m_mqttCaCert; }
