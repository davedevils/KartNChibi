#include "capture.h"
#include "safe_mem.h"

namespace probe {

static uint32_t source_value(const CaptureDesc& d, const HookContext& ctx) {
    switch (d.source) {
        case SourceKind::Eax: return ctx.eax;
        case SourceKind::Ecx: return ctx.ecx;
        case SourceKind::Edx: return ctx.edx;
        case SourceKind::Arg: return ctx.stack[1 + d.source_index];
        case SourceKind::EspOff:
            return *reinterpret_cast<const uint32_t*>(
                reinterpret_cast<const uint8_t*>(ctx.stack) + d.source_index);
        case SourceKind::Abs: return d.source_index;
    }
    return 0;
}

bool resolve_capture(const CaptureDesc& d, const HookContext& ctx,
                     uint32_t max_bytes, CaptureBlob& out) {
    uint32_t size = d.size;
    if (size > max_bytes) size = max_bytes;
    if (size > static_cast<uint32_t>(kMaxCaptureBytes)) size = kMaxCaptureBytes;

    uint32_t value = source_value(d, ctx);

    if (d.mode == CaptureMode::Val) {
        const uint32_t n = size < 4 ? size : 4;
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&value);
        for (uint32_t i = 0; i < n; i++) out.data[i] = p[i];
        out.size = n;
        return true;
    }

    for (uint32_t off : d.deref) {
        uint32_t next = 0;
        if (safe_read(reinterpret_cast<const void*>(value + off), &next, 4) != 4) {
            out.size = 0;
            return false;
        }
        value = next;
    }
    const size_t n = safe_read(reinterpret_cast<const void*>(value), out.data, size);
    out.size = static_cast<uint32_t>(n);
    return n == size;
}

}  // namespace probe
