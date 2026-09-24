#pragma once
#include <cstdint>
#include "config.h"
#include "ring_buffer.h"

namespace probe {

// installs a MinHook hook for each entry in cfg routing captures into ring
bool install_hooks(const ProbeConfig& cfg, RingBuffer& ring, uint32_t runtime_base);
void uninstall_hooks();

}  // namespace probe
