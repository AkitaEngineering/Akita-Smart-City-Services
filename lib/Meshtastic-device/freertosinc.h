// Minimal freertosinc.h stub to satisfy Meshtastic-device includes during
// example builds. This is intentionally minimal and may need extension
// if Meshtastic code relies on FreeRTOS types or APIs.
#ifndef FREERTOSINC_H
#define FREERTOSINC_H

// Provide minimal FreeRTOS type aliases used in Meshtastic headers.
typedef void* TaskHandle_t;
typedef int BaseType_t;
typedef unsigned int UBaseType_t;

// Minimal macros used by Meshtastic-device build presence checks.
#define pdTRUE 1
#define pdFALSE 0

#endif // FREERTOSINC_H
