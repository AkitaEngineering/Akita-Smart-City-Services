#ifndef AKITASMARTCITYSERVICES_H
#define AKITASMARTCITYSERVICES_H

#include "meshtastic.h"
#include "SmartCity.pb.h"
#include <PubSubClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <vector>
#include <Preferences.h>

#define SMART_CITY_PORT 123 // Define your port number
#define BROADCAST_ADDR 0xFFFFFFFF // Define broadcast address

class AkitaSmartCityServices {
public:
    void begin();
    void update();

private:
    void loadConfig();
    void connectWiFi();
    void connectMQTT();
    static void mqttCallback(char *topic, byte *payload, unsigned int length); // Static callback
    void handleReceivedPacket(const meshPacket &packet);
    void handleServiceDiscovery(ServiceDiscovery &discovery, uint32_t fromNode);
    void handleSensorData(SensorData &sensorData, uint32_t fromNode);
    void publishMqtt(SensorData &sensorData, uint32_t fromNode);
    void sendServiceDiscovery();
    void sendMessage(uint32_t to, SmartCityMessage &msg);

    struct ServiceInfo {
        uint32_t serviceId;
        uint32_t nodeId;
        unsigned long lastSeen;
    };

    std::vector<ServiceInfo> serviceTable;

    uint32_t myServiceId;
    uint32_t myNodeRole;

    unsigned long lastServiceDiscovery;
    unsigned long lastMqttReconnect;
};

#endif // AKITASMARTCITYSERVICES_H
