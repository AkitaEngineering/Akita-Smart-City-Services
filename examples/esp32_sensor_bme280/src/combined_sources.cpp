// Combined compilation unit to include shared library sources from repository root
// This forces PlatformIO to compile the shared C/C++ sources which live outside
// the example `src/` folder.

#include "../../src/AkitaSmartCityServices.cpp"
#include "../../src/ASCSConfig.cpp"
#include "../../src/meshtastic.cpp"
#include "../../src/generated_proto/SmartCity.pb.c"
