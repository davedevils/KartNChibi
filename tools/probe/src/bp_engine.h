#pragma once
#include <cstdint>
#include <string>
#include "bp_queue.h"
namespace probe {
void bp_init(BpQueue* queue);
void bp_shutdown();
bool bp_arm(uint32_t addr, bool suspend, bool int3);
bool bp_disarm(uint32_t addr);
void bp_resume(uint32_t addr, bool all);
std::string bp_list();
}  // namespace probe
