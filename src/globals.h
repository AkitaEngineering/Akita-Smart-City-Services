// Adapter to prefer upstream SDK globals if present, otherwise local shim
#ifndef GLOBALS_H
#define GLOBALS_H

#if defined(__has_include)
#  if __has_include(<meshtastic/globals.h>)
#    include <meshtastic/globals.h>
#  else
#    include "shims/globals.h"
#  endif
#else
#  include "shims/globals.h"
#endif

#endif // GLOBALS_H
