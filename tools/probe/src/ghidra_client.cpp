#include "ghidra_client.h"
#include <cstdio>
#include <winsock2.h>
#include <ws2tcpip.h>
namespace probe {
static std::string json_escape(const std::string& v) {
    std::string o;
    for (char c : v) { if (c == '"' || c == '\\') o += '\\'; if (c == '\n') { o += "\\n"; continue; } o += c; }
    return o;
}
std::string build_annotate_request(uint32_t addr, const std::string& comment) {
    char ahex[11]; std::snprintf(ahex, sizeof ahex, "0x%08x", addr);
    std::string body = std::string("{\"address\":\"") + ahex + "\",\"comment\":\"" + json_escape(comment) + "\"}";
    std::string req = "POST /set_disassembly_comment HTTP/1.1\r\n";
    req += "Host: 127.0.0.1:8089\r\n";
    req += "Content-Type: application/json\r\n";
    req += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    req += "Connection: close\r\n\r\n";
    req += body;
    return req;
}
bool ghidra_annotate(uint32_t addr, const std::string& comment) {
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return false;
    sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons(8089);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bool ok = false;
    if (connect(s, reinterpret_cast<sockaddr*>(&a), sizeof a) == 0) {
        const std::string req = build_annotate_request(addr, comment);
        ok = send(s, req.c_str(), static_cast<int>(req.size()), 0) == static_cast<int>(req.size());
    }
    closesocket(s);
    return ok;
}
}  // namespace probe
