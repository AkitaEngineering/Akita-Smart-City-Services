// Minimal Preferences stub
#ifndef PREFERENCES_STUB_H
#define PREFERENCES_STUB_H

#include <string>
#include <unordered_map>

class Preferences {
public:
    Preferences() = default;
    ~Preferences() = default;
    bool begin(const char*, bool) { return true; }
    void end() {}
    unsigned int getUInt(const char*, unsigned int def) { return def; }
    int getInt(const char*, int def) { return def; }
    std::string getString(const char*, const char* def) { return std::string(def); }
    void putUInt(const char*, unsigned int) {}
    void putInt(const char*, int) {}
    void putString(const char*, const std::string&) {}
    std::string getString(const std::string&, const std::string& def) { return def; }

private:
    std::unordered_map<std::string, std::string> m_store;
};

#endif // PREFERENCES_STUB_H
