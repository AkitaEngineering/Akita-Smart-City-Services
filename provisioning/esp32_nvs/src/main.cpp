#include <Arduino.h>
#include <Preferences.h>
#include <cctype>
#include "provisioning.h"

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
    if (length == 0 || length > 253) return false;
    for (size_t index = 0; index < length; ++index) {
        const unsigned char character = static_cast<unsigned char>(host[index]);
        if (std::isspace(character) || std::iscntrl(character) || character == '/' || character == '\\') return false;
    }
    return true;
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

bool provision() {
    if (!validConfiguration()) return false;
    Preferences preferences;
    if (!preferences.begin("ascs", false)) return false;

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
