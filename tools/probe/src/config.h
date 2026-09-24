#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace probe {

enum class SourceKind { Eax, Ecx, Edx, Arg, EspOff, Abs };
enum class CaptureMode { Ptr, Val };

struct CaptureDesc {
    std::string label;
    SourceKind source = SourceKind::Eax;
    uint32_t source_index = 0;        // arg index esp byte offset or an absolute address
    CaptureMode mode = CaptureMode::Ptr;
    std::vector<uint32_t> deref;
    uint32_t size = 0;
};

struct HookDesc {
    std::string name;
    uint32_t address = 0;             // config file address before rebase dir 0 inbound 1 outbound 2 state or poll
    uint8_t dir = 0;
    bool is_poll = false;
    uint32_t interval_ms = 0;
    std::vector<CaptureDesc> captures;
};

struct ProbeConfig {
    std::string module;
    uint32_t image_base = 0;
    std::string jsonl_path;
    uint16_t tcp_port = 0;
    uint32_t ring_capacity = 0;
    uint32_t max_capture_bytes = 0;
    std::vector<HookDesc> hooks;
};

bool parse_config(const std::string& text, ProbeConfig& out, std::string& error);
uint32_t rebase_address(uint32_t config_addr, uint32_t config_base, uint32_t runtime_base);
// shifts every Abs source capture address by runtime base minus image base in place
void rebase_abs_captures(ProbeConfig& cfg, uint32_t runtime_base);

}  // namespace probe
