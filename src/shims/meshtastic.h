// Copied meshtastic shim (moved) â€” used when upstream SDK isn't vendored.
#ifndef MESHTASTIC_H
#define MESHTASTIC_H

#include "plugin_api.h"
#include "mesh_portnums.h"
#include <cstdint>
#include <vector>
#include <algorithm>

struct meshPacketDecoded { PortNum portnum = (PortNum)0; const uint8_t* payload = nullptr; size_t payloadlen = 0; };
struct meshPacket { uint32_t from = 0; meshPacketDecoded decoded; };

enum MeshServiceDiscovery_Role { MESH_SERVICE_ROLE_UNKNOWN = 0, MESH_SERVICE_ROLE_SENSOR = 1, MESH_SERVICE_ROLE_AGGREGATOR = 2, MESH_SERVICE_ROLE_GATEWAY = 3 };
struct MeshServiceDiscovery { MeshServiceDiscovery_Role node_role = MESH_SERVICE_ROLE_UNKNOWN; uint32_t service_id = 0; };

#define Data_WANT_ACK_DEFAULT 0

class MeshInterface { public: virtual bool sendData(uint32_t toNode, const uint8_t* data, size_t len, PortNum port, int wantAck, int ttl) { (void)toNode; (void)data; (void)len; (void)port; (void)wantAck; (void)ttl; return true; } virtual ~MeshInterface() {} };

struct NodeInfo { uint32_t node_num = 0; };

class MeshtasticAPI { public: MeshtasticAPI() : m_primary(nullptr) {} void setPrimaryInterface(MeshInterface* iface) { m_primary = iface; } MeshInterface* getPrimaryInterface() const { return m_primary; } NodeInfo* getMyNodeInfo() { return &m_nodeInfo; } const NodeInfo* getMyNodeInfo() const { return &m_nodeInfo; } unsigned long getAdjustedTime() const { return 0; } private: MeshInterface* m_primary; NodeInfo m_nodeInfo; };

class MeshtasticHost { public: MeshtasticHost() { m_api.setPrimaryInterface(&m_iface); } void addPlugin(MeshtasticPlugin* p) { if (!p) return; m_plugins.push_back(p); } void begin() { for (auto *p : m_plugins) p->init(&m_api); } void loop() { for (auto *p : m_plugins) p->loop(); } MeshtasticAPI* api() { return &m_api; } private: MeshtasticAPI m_api; MeshInterface m_iface; std::vector<MeshtasticPlugin*> m_plugins; };

extern MeshtasticHost meshtastic;

#endif // MESHTASTIC_H
