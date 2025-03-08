// AkitaSmartCityServices.cpp - Meshtastic Plugin for Smart City IoT

#include "AkitaSmartCityServices.h"
#include "meshtastic.h"
#include "pb_encode.h"
#include "pb_decode.h"
#include "SmartCity.pb.h"
#include <PubSubClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>

extern Meshtastic meshtastic;

// Configuration
#define MQTT_SERVER "your_mqtt_server.com"
#define MQTT_PORT 1883
#define MQTT_USER "your_mqtt_user"
#define MQTT_PASSWORD "your_mqtt_password"
#define WIFI_SSID "your_wifi_ssid"
#define WIFI_PASSWORD "your_wifi_password"

#define SERVICE_DISCOVERY_INTERVAL 60000 // 1 minute
#define MQTT_RECONNECT_INTERVAL 10000 // 10 seconds

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

struct ServiceInfo {
  uint32_t serviceId;
  uint32_t nodeId;
};

std::vector<ServiceInfo> serviceTable;

uint32_t myServiceId = 0; // Configurable
uint32_t myNodeRole = 0; // Configurable, 0: Sensor, 1: Aggregator, 2: Gateway

unsigned long lastServiceDiscovery = 0;
unsigned long lastMqttReconnect = 0;

void AkitaSmartCityServices::begin() {
  Serial.println("AkitaSmartCityServices Plugin Started");

  // Load configuration from preferences or external source
  loadConfig();

  // Initialize WiFi and MQTT if gateway
  if (myNodeRole == 2) {
    connectWiFi();
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    connectMQTT();
  }

  // Register message handler
  meshtastic.addReceiveHook([this](const meshPacket &packet) {
    handleReceivedPacket(packet);
  });

  // Start service discovery timer
  lastServiceDiscovery = millis();
}

void AkitaSmartCityServices::loadConfig() {
  // Load myServiceId and myNodeRole from preferences
  myServiceId = 1;
  myNodeRole = 0;
}

void AkitaSmartCityServices::connectWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void AkitaSmartCityServices::connectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "MeshtasticClient-" + String(random(0xffff), HEX);
    if (mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void AkitaSmartCityServices::mqttCallback(char *topic, byte *payload, unsigned int length) {
}

void AkitaSmartCityServices::handleReceivedPacket(const meshPacket &packet) {
  if (packet.decoded.portnum == SMART_CITY_PORT) {
    SmartCityMessage msg = SmartCityMessage_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(packet.decoded.payload, packet.decoded.payloadlen);
    if (pb_decode(&stream, SmartCityMessage_fields, &msg)) {
      if (msg.which_payload == SmartCityMessage_serviceDiscovery_tag) {
        handleServiceDiscovery(msg.payload.serviceDiscovery, packet.from);
      } else if (msg.which_payload == SmartCityMessage_sensorData_tag) {
        handleSensorData(msg.payload.sensorData, packet.from);
      }
    } else {
      Serial.println("Failed to decode SmartCityMessage");
    }
  }
}

void AkitaSmartCityServices::handleServiceDiscovery(ServiceDiscovery &discovery, uint32_t fromNode) {
  if (discovery.request) {
    if (myServiceId != 0) {
      SmartCityMessage response = SmartCityMessage_init_zero;
      response.which_payload = SmartCityMessage_serviceDiscovery_tag;
      response.payload.serviceDiscovery.response = true;
      response.payload.serviceDiscovery.serviceId = myServiceId;
      sendMessage(fromNode, response);
    }
  } else if (discovery.response) {
    ServiceInfo serviceInfo = {discovery.serviceId, fromNode};
    serviceTable.push_back(serviceInfo);
    Serial.print("Discovered service: ");
    Serial.print(discovery.serviceId);
    Serial.print(" from node: ");
    Serial.println(fromNode);
  }
}

void AkitaSmartCityServices::handleSensorData(SensorData &sensorData, uint32_t fromNode) {
  if (myNodeRole == 1 || myNodeRole == 2) {
    if (myNodeRole == 2) {
      publishMqtt(sensorData, fromNode);
    } else {
      for(ServiceInfo& service : serviceTable){
          if(service.serviceId == myServiceId){
            SmartCityMessage forwardMessage = SmartCityMessage_init_zero;
            forwardMessage.which_payload = SmartCityMessage_sensorData_tag;
            forwardMessage.payload.sensorData = sensorData;
            sendMessage(service.nodeId, forwardMessage);
            break;
          }
      }
    }
  }
}

void AkitaSmartCityServices::publishMqtt(SensorData &sensorData, uint32_t fromNode) {
  if (mqttClient.connected()) {
    String topic = "smartcity/" + String(myServiceId) + "/" + String(fromNode);
    String payload;
    StaticJsonDocument doc;
    doc["temperature"] = sensorData.temperature;
    doc["humidity"] = sensorData.humidity;
    serializeJson(doc, payload);
    mqttClient.publish(topic.c_str(), payload.c_str());
  }
}

void AkitaSmartCityServices::update() {
  if (millis() - lastServiceDiscovery > SERVICE_DISCOVERY_INTERVAL) {
    sendServiceDiscovery();
    lastServiceDiscovery = millis();
  }

  if (myNodeRole == 2) {
    if (WiFi.status() != WL_CONNECTED) {
      connectWiFi();
    }

    if (!mqttClient.connected()) {
      if (millis() - lastMqttReconnect > MQTT_RECONNECT_INTERVAL){
        connectMQTT();
        lastMqttReconnect = millis();
      }
    } else {
      mqttClient.loop();
    }
  }
}

void AkitaSmartCityServices::sendServiceDiscovery() {
  SmartCityMessage msg = SmartCityMessage_init_zero;
  msg.which_payload = SmartCityMessage_serviceDiscovery_tag;
  msg.payload.serviceDiscovery.request = true;
  sendMessage(BROADCAST_ADDR, msg);
}

void AkitaSmartCityServices::sendMessage(uint32_t to, SmartCityMessage &msg) {
  uint8_t buffer[256];
  pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
  if (pb_encode(&stream, SmartCityMessage_fields, &msg)) {
    meshtastic.sendData(to, buffer, stream.bytes_written, SMART_CITY_PORT);
  } else {
    Serial.println("Failed to encode SmartCityMessage");
  }
}
