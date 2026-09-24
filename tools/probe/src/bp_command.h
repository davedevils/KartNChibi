#pragma once
#include <cstdint>
#include <string>
namespace probe {
enum class BpCmdKind { None, Arm, Remove, Resume, List };
// defaults are trace not suspend and hardware not int3 resume all or one address
struct BpCommand {
    BpCmdKind kind = BpCmdKind::None;
    uint32_t addr = 0;
    bool suspend = false;
    bool int3 = false;
    bool resume_all = false;
};
BpCommand parse_bp_command(const std::string& line);
}  // namespace probe
