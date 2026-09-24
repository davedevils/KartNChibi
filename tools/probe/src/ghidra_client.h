#pragma once
#include <cstdint>
#include <string>
namespace probe {
std::string build_annotate_request(uint32_t addr, const std::string& comment);
bool ghidra_annotate(uint32_t addr, const std::string& comment);  // winsock best effort
}
