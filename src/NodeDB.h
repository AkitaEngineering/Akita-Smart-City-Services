// Minimal stub for NodeDB to satisfy builds when Meshtastic host headers
// are not available in the build environment. This provides only the
// declarations used by examples and should be replaced with the real
// Meshtastic NodeDB when integrating with the full SDK.
#ifndef NODEDB_H
#define NODEDB_H

class NodeDB {
public:
    static void begin() {}
};

#endif // NODEDB_H
