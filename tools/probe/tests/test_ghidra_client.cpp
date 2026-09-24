#include "ghidra_client.h"
#include <cassert>
#include <string>
using namespace probe;
int main() {
    const std::string req = build_annotate_request(0x00428390, "hit eax=1");
    assert(req.rfind("POST /set_disassembly_comment HTTP/1.1\r\n", 0) == 0);
    assert(req.find("Host: 127.0.0.1:8089\r\n") != std::string::npos);
    assert(req.find("Content-Type: application/json\r\n") != std::string::npos);
    assert(req.find("Connection: close\r\n") != std::string::npos);
    const std::string body = "{\"address\":\"0x00428390\",\"comment\":\"hit eax=1\"}";
    assert(req.find("Content-Length: " + std::to_string(body.size()) + "\r\n") != std::string::npos);
    assert(req.size() >= body.size() && req.compare(req.size() - body.size(), body.size(), body) == 0);
    assert(req.find("\r\n\r\n") != std::string::npos);  // header body separator
    return 0;
}
