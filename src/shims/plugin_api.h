// Copied shim (moved) â€” original file preserved here for compatibility.
#ifndef PLUGIN_API_H
#define PLUGIN_API_H

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <string>

// Log level constants used across the project.
#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_INFO 1
#define LOG_LEVEL_WARNING 2
#define LOG_LEVEL_ERROR 3
#define LOG_LEVEL_CRITICAL 4
#define LOG_LEVEL_TRACE 5

struct MeshtasticAPI;
struct meshPacket;

class MeshtasticPlugin {
public:
    MeshtasticPlugin(const char *name = nullptr) : m_name(name ? name : "") {}
    virtual void init(const MeshtasticAPI *api) {}
    virtual void loop() {}
    virtual bool handleReceived(const meshPacket *packet) { (void)packet; return false; }
    virtual ~MeshtasticPlugin() {}
    const char* getName() const { return m_name.c_str(); }
protected:
    std::string m_name;
};

static struct PluginLog {
    const char* levelToString(int level) {
        switch (level) {
            case LOG_LEVEL_DEBUG: return "DEBUG";
            case LOG_LEVEL_INFO: return "INFO";
            case LOG_LEVEL_WARNING: return "WARN";
            case LOG_LEVEL_ERROR: return "ERROR";
            case LOG_LEVEL_CRITICAL: return "CRIT";
            case LOG_LEVEL_TRACE: return "TRACE";
            default: return "LOG";
        }
    }
    void vprint_with_prefix(int level, const char *fmt, va_list ap) {
        const char *lvl = levelToString(level);
        ::printf("[%s] ", lvl);
        ::vprintf(fmt, ap);
    }
    void printf(int level, const char *fmt, ...) {
        va_list ap; va_start(ap, fmt); vprint_with_prefix(level, fmt, ap); va_end(ap);
    }
    void println(int level, const char *fmt, ...) {
        va_list ap; va_start(ap, fmt); vprint_with_prefix(level, fmt, ap); va_end(ap);
        size_t len = strlen(fmt); if (len == 0 || fmt[len-1] != '\n') ::printf("\n");
    }
} Log;

#endif // PLUGIN_API_H
