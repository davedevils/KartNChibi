#pragma once
#include <string>
#include "bp_hit.h"
namespace probe {
std::string serialize_bp_hit(const BpHit& hit);
}  // namespace probe
