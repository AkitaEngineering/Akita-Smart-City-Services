#pragma once
#if defined(__has_include)
#  if __has_include(<meshtastic/mesh_packet.h>)
#    include <meshtastic/mesh_packet.h>
#  else
#    include "shims/mesh_packet.h"
#  endif
#else
#  include "shims/mesh_packet.h"
#endif
