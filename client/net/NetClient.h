// byte exact with our server wire reads frames and logs both directions
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "net/Packet.h"
#include "net/Protocol.h"

namespace KnC::Client {

using ::knc::Packet;

// one wire frame with the direction it travelled and when
struct WireEvent {
    bool     outbound = false;
    double   t = 0.0;
    uint16_t opcode = 0;
    std::vector<uint8_t> payload;
};

// text log of every frame one header line then a hex body
class WireLog {
public:
    bool open(const std::string& path);
    void record(const WireEvent& ev);
    void close();
    // frames also go to stdout when true the headless wants that
    void setEcho(bool echo) { m_echo = echo; }
private:
    FILE* m_f = nullptr;
    bool m_echo = true;
};

class NetClient {
public:
    explicit NetClient(WireLog* log = nullptr) : m_log(log) {}
    ~NetClient();
    NetClient(const NetClient&) = delete;
    NetClient& operator=(const NetClient&) = delete;
    void setLog(WireLog* log) { m_log = log; }

    bool connect(const std::string& host, uint16_t port);
    void disconnect();
    bool connected() const { return m_sock != INVALID_SOCKET; }

    bool send(const Packet& pkt);

    // drains the socket reassembles and logs frames returns false when the peer closed
    bool pump(int timeoutMs);

    // called for every inbound frame after it is logged
    std::function<void(uint16_t opcode, Packet&)> onFrame;

    double now() const;

    // bytes of every frame dispatched since the last take the 0xA6 keepalive reports it
    uint32_t takeConsumed();

private:
    SOCKET m_sock = INVALID_SOCKET;
    WireLog* m_log = nullptr;
    std::vector<uint8_t> m_rx;
    long long m_startTick = 0;
    uint32_t m_consumed = 0;
    static bool s_wsaUp;
    static void ensureWsa();
};

}
