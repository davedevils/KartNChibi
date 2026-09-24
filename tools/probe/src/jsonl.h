#pragma once
#include <string>
#include "config.h"
#include "probe_record.h"

namespace probe {

// serializes one record to a single JSON line with no trailing newline
std::string serialize_record(const ProbeRecord& rec, const ProbeConfig& cfg);

}  // namespace probe
