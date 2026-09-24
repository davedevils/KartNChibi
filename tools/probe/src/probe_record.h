#pragma once
#include <cstdint>

namespace probe {

constexpr int kMaxCaptureBytes = 16;
constexpr int kMaxCapturesPerHook = 128;
constexpr int kMaxHooks = 16;
constexpr uint16_t kNoOpcode = 0xFFFF;

struct CaptureBlob {
    uint32_t size;                    // valid bytes in data
    uint8_t  data[kMaxCaptureBytes];
};

struct ProbeRecord {
    uint64_t seq;
    uint64_t t_us;
    uint32_t thread_id;
    uint16_t hook_id;
    uint16_t opcode;                  // kNoOpcode when absent
    uint8_t  blob_count;
    CaptureBlob blobs[kMaxCapturesPerHook];
};

}  // namespace probe
