#pragma once
#include <cstdint>
#include "config.h"
#include "probe_record.h"

namespace probe {

struct HookContext {
    uint32_t eax = 0;
    uint32_t ecx = 0;
    uint32_t edx = 0;
    const uint32_t* stack = nullptr;  // stack 0 is the return address and stack 1 plus N is arg N
};

// resolves one descriptor into out and returns false on a faulting read with size zero
bool resolve_capture(const CaptureDesc& desc, const HookContext& ctx,
                     uint32_t max_bytes, CaptureBlob& out);

}  // namespace probe
