#pragma once
#include <cstdint>
namespace probe {
constexpr int kBpStackBytes = 128;
constexpr int kBpCodeBytes = 16;
// regs order is eax ecx edx ebx esp ebp esi edi
struct BpHit {
    uint64_t seq;
    uint64_t t_us;
    uint32_t thread_id;
    uint32_t addr;
    uint8_t  mode;          // 0 is trace and 1 is suspend
    uint32_t regs[8];
    uint32_t eip;
    uint32_t eflags;
    uint32_t stack_len;
    uint32_t code_len;
    uint8_t  stack[kBpStackBytes];
    uint8_t  code[kBpCodeBytes];
};
}  // namespace probe
