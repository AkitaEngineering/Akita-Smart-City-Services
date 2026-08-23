#include <Arduino.h>
#include <Preferences.h>
#include <cctype>
#include "provisioning.h"

#if ASCS_PROVISION_SCHEMA_VERSION != 1U
#error "This provisioner only supports ASCS configuration schema version 1"
#endif

namespace {
bool validMqttTopic(const char *topic) {
    if (!topic) return false;
    const size_t length = strlen(topic);
    if (length == 0 || length > 128 || topic[0] == '/' || topic[length - 1] == '/') return false;
    bool previousSlash = false;
    for (size_t index = 0; index < length; ++index) {
        const unsigned char character = static_cast<unsigned char>(topic[index]);
        if (character == '#' || character == '+' || std::isspace(character) || std::iscntrl(character)) return false;
        if (character == '/') {
            if (previousSlash) return false;
            previousSlash = true;
        } else {
            previousSlash = false;
        }
    }
    return true;
}

bool validMqttHost(const char *host) {
    if (!host) return false;
    const size_t length = strlen(host);
    if (length == 0 || length > 253 || host[0] == '.' || host[length - 1] == '.') return false;
    size_t labelLength = 0;
    bool previousHyphen = false;
    for (size_t index = 0; index < length; ++index) {
        const unsigned char character = static_cast<unsigned char>(host[index]);
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

bool validConfiguration() {
    if (ASCS_PROVISION_ROLE < 1U || ASCS_PROVISION_ROLE > 3U || ASCS_PROVISION_SERVICE_ID == 0U) return false;
    if (ASCS_PROVISION_READ_INTERVAL_MS < 1000U || ASCS_PROVISION_DISCOVERY_INTERVAL_MS < 1000U) return false;
    if (ASCS_PROVISION_SERVICE_TIMEOUT_MS <= ASCS_PROVISION_DISCOVERY_INTERVAL_MS ||
        ASCS_PROVISION_MQTT_RECONNECT_MS < 1000U) return false;
    if (ASCS_PROVISION_ROLE == 3U) {
        return ASCS_PROVISION_WIFI_SSID[0] != '\0' && ASCS_PROVISION_MQTT_SERVER[0] != '\0' &&
               ASCS_PROVISION_MQTT_USER[0] != '\0' && ASCS_PROVISION_MQTT_PASSWORD[0] != '\0' &&
               ASCS_PROVISION_MQTT_CA_CERT[0] != '\0' && ASCS_PROVISION_MQTT_PORT > 0U &&
               ASCS_PROVISION_MQTT_PORT <= 65535U && strlen(ASCS_PROVISION_WIFI_SSID) <= 32U &&
               strlen(ASCS_PROVISION_WIFI_PASSWORD) <= 64U && validMqttHost(ASCS_PROVISION_MQTT_SERVER) &&
               strlen(ASCS_PROVISION_MQTT_USER) <= 256U && strlen(ASCS_PROVISION_MQTT_PASSWORD) <= 4096U &&
               strlen(ASCS_PROVISION_MQTT_CA_CERT) <= 8192U && validMqttTopic(ASCS_PROVISION_MQTT_TOPIC);
    }
    return true;
}

bool writeString(Preferences &preferences, const char *key, const char *value) {
    return preferences.putString(key, value) == strlen(value);
}

bool storedStringEquals(Preferences &preferences, const char *key, const char *expected) {
    return preferences.getString(key, "").equals(expected);
}

bool provision() {
    if (!validConfiguration()) return false;
    Preferences preferences;
    if (!preferences.begin("ascs", false)) return false;

    const bool markerInvalidated = !preferences.isKey("schema_ver") || preferences.remove("schema_ver");
    if (!markerInvalidated || preferences.isKey("schema_ver")) {
        preferences.end();
        return false;
    }
    bool success = preferences.putUInt("role", ASCS_PROVISION_ROLE) == sizeof(uint32_t);
    success = preferences.putUInt("service_id", ASCS_PROVISION_SERVICE_ID) == sizeof(uint32_t) && success;
    success = preferences.putUInt("target_node", ASCS_PROVISION_TARGET_NODE) == sizeof(uint32_t) && success;
    success = preferences.putUInt("trusted_gw", ASCS_PROVISION_TRUSTED_GATEWAY_NODE) == sizeof(uint32_t) && success;
    success = preferences.putUInt("read_int", ASCS_PROVISION_READ_INTERVAL_MS) == sizeof(uint32_t) && success;
    success = preferences.putUInt("disc_int", ASCS_PROVISION_DISCOVERY_INTERVAL_MS) == sizeof(uint32_t) && success;
    success = preferences.putUInt("svc_tout", ASCS_PROVISION_SERVICE_TIMEOUT_MS) == sizeof(uint32_t) && success;
    success = preferences.putUInt("mqtt_rec_int", ASCS_PROVISION_MQTT_RECONNECT_MS) == sizeof(uint32_t) && success;
    if (ASCS_PROVISION_ROLE == 3U) {
        success = writeString(preferences, "wifi_ssid", ASCS_PROVISION_WIFI_SSID) && success;
        success = writeString(preferences, "wifi_pass", ASCS_PROVISION_WIFI_PASSWORD) && success;
        success = writeString(preferences, "mqtt_srv", ASCS_PROVISION_MQTT_SERVER) && success;
        success = preferences.putUInt("mqtt_port", ASCS_PROVISION_MQTT_PORT) == sizeof(uint32_t) && success;
        success = writeString(preferences, "mqtt_user", ASCS_PROVISION_MQTT_USER) && success;
        success = writeString(preferences, "mqtt_pass", ASCS_PROVISION_MQTT_PASSWORD) && success;
        success = writeString(preferences, "mqtt_topic", ASCS_PROVISION_MQTT_TOPIC) && success;
        success = writeString(preferences, "mqtt_ca", ASCS_PROVISION_MQTT_CA_CERT) && success;
    }

    success = success && preferences.getUInt("role", 0) == ASCS_PROVISION_ROLE;
    success = success && preferences.getUInt("service_id", 0) == ASCS_PROVISION_SERVICE_ID;
    success = success && preferences.getUInt("target_node", UINT32_MAX) == ASCS_PROVISION_TARGET_NODE;
    success = success && preferences.getUInt("trusted_gw", UINT32_MAX) == ASCS_PROVISION_TRUSTED_GATEWAY_NODE;
    success = success && preferences.getUInt("read_int", 0) == ASCS_PROVISION_READ_INTERVAL_MS;
    success = success && preferences.getUInt("disc_int", 0) == ASCS_PROVISION_DISCOVERY_INTERVAL_MS;
    success = success && preferences.getUInt("svc_tout", 0) == ASCS_PROVISION_SERVICE_TIMEOUT_MS;
    success = success && preferences.getUInt("mqtt_rec_int", 0) == ASCS_PROVISION_MQTT_RECONNECT_MS;
    if (ASCS_PROVISION_ROLE == 3U) {
        success = success && storedStringEquals(preferences, "wifi_ssid", ASCS_PROVISION_WIFI_SSID);
        success = success && storedStringEquals(preferences, "wifi_pass", ASCS_PROVISION_WIFI_PASSWORD);
        success = success && storedStringEquals(preferences, "mqtt_srv", ASCS_PROVISION_MQTT_SERVER);
        success = success && preferences.getUInt("mqtt_port", 0) == ASCS_PROVISION_MQTT_PORT;
        success = success && storedStringEquals(preferences, "mqtt_user", ASCS_PROVISION_MQTT_USER);
        success = success && storedStringEquals(preferences, "mqtt_pass", ASCS_PROVISION_MQTT_PASSWORD);
        success = success && storedStringEquals(preferences, "mqtt_topic", ASCS_PROVISION_MQTT_TOPIC);
        success = success && storedStringEquals(preferences, "mqtt_ca", ASCS_PROVISION_MQTT_CA_CERT);
    }
    if (success) {
        success = preferences.putUInt("schema_ver", ASCS_PROVISION_SCHEMA_VERSION) == sizeof(uint32_t) &&
                  preferences.getUInt("schema_ver", 0) == ASCS_PROVISION_SCHEMA_VERSION;
    }
    if (!success && preferences.isKey("schema_ver")) preferences.remove("schema_ver");
    preferences.end();
    return success;
}
}

void setup() {
    Serial.begin(115200);
    delay(1500);
    const bool success = provision();
    Serial.println(success ? "ASCS provisioning completed. Flash production firmware without erasing NVS."
                           : "ASCS provisioning failed. Configuration was invalid or NVS could not be written.");
}

void loop() { delay(1000); }
