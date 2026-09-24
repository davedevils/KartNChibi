#include "NetClient.h"

#include <chrono>
#include <share.h>

#pragma comment(lib, "ws2_32.lib")

namespace KnC::Client {

bool NetClient::s_wsaUp = false;

void NetClient::ensureWsa() {
    if (s_wsaUp) return;
    WSADATA d{};
    WSAStartup(MAKEWORD(2, 2), &d);
    s_wsaUp = true;
}

static long long tickMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

NetClient::~NetClient() { disconnect(); }

double NetClient::now() const {
    return (tickMs() - m_startTick) / 1000.0;
}

bool NetClient::connect(const std::string& host, uint16_t port) {
    ensureWsa();
    disconnect();
    m_startTick = m_startTick ? m_startTick : tickMs();

    m_sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_sock == INVALID_SOCKET) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        // fall back to a name lookup so a hostname target still works
        addrinfo hints{}; hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
        addrinfo* res = nullptr;
        if (::getaddrinfo(host.c_str(), nullptr, &hints, &res) != 0 || !res) {
            disconnect(); return false;
        }
        addr.sin_addr = reinterpret_cast<sockaddr_in*>(res->ai_addr)->sin_addr;
        ::freeaddrinfo(res);
    }
    if (::connect(m_sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        disconnect(); return false;
    }
    // sub 4769C0 sets TCP NODELAY so a 0x0040 report leaves at once never with the next
    const char noDelay = 1;
    ::setsockopt(m_sock, IPPROTO_TCP, TCP_NODELAY, &noDelay, sizeof(noDelay));
    m_rx.clear();
    m_consumed = 0;
    return true;
}

void NetClient::disconnect() {
    if (m_sock != INVALID_SOCKET) {
        ::closesocket(m_sock);
        m_sock = INVALID_SOCKET;
    }
}

bool NetClient::send(const Packet& pkt) {
    if (!connected()) return false;
    std::vector<uint8_t> bytes = pkt.serialize();
    size_t off = 0;
    while (off < bytes.size()) {
        int n = ::send(m_sock, reinterpret_cast<const char*>(bytes.data() + off),
                       static_cast<int>(bytes.size() - off), 0);
        if (n <= 0) { disconnect(); return false; }
        off += n;
    }
    if (m_log) {
        WireEvent ev;
        ev.outbound = true; ev.t = now(); ev.opcode = pkt.cmdFull();
        ev.payload = pkt.payload();
        m_log->record(ev);
    }
    return true;
}

bool NetClient::pump(int timeoutMs) {
    if (!connected()) return false;

    // drains up to 64 reads per call so a login burst lands in a few frames
    for (int reads = 0; reads < 64; ++reads) {
        fd_set rs; FD_ZERO(&rs); FD_SET(m_sock, &rs);
        timeval tv{ timeoutMs / 1000, (timeoutMs % 1000) * 1000 };
        int sel = ::select(0, &rs, nullptr, nullptr, &tv);
        if (sel <= 0 || !FD_ISSET(m_sock, &rs)) break;
        char buf[8192];
        int n = ::recv(m_sock, buf, sizeof(buf), 0);
        if (n <= 0) { disconnect(); return false; }
        m_rx.insert(m_rx.end(), buf, buf + n);
        timeoutMs = 0;
        if (n < static_cast<int>(sizeof(buf))) break;
    }

    // pull every complete frame out of the reassembly buffer
    for (;;) {
        if (m_rx.size() < ::knc::PACKET_HEADER_SIZE) break;
        size_t need = Packet::peekSize(m_rx.data(), m_rx.size());
        if (need == 0 || m_rx.size() < need) break;
        auto parsed = Packet::parse(m_rx.data(), need);
        m_rx.erase(m_rx.begin(), m_rx.begin() + need);
        if (!parsed) continue;
        Packet pkt = std::move(*parsed);
        m_consumed += static_cast<uint32_t>(need);
        if (m_log) {
            WireEvent ev;
            ev.outbound = false; ev.t = now(); ev.opcode = pkt.cmdFull();
            ev.payload = pkt.payload();
            m_log->record(ev);
        }
        if (onFrame) onFrame(pkt.cmdFull(), pkt);
        // a handler may have closed the socket the buffer is stale then
        if (!connected()) return false;
    }
    return true;
}

uint32_t NetClient::takeConsumed() {
    const uint32_t n = m_consumed;
    m_consumed = 0;
    return n;
}

bool WireLog::open(const std::string& path) {
    // share the handle so the log can be tailed live a plain fopen locks it on windows
    m_f = _fsopen(path.c_str(), "w", _SH_DENYNO);
    return m_f != nullptr;
}

void WireLog::record(const WireEvent& ev) {
    const char* dir = ev.outbound ? "TX" : "RX";
    if (m_f) {
        fprintf(m_f, "[%9.3f] %s 0x%04X size=%zu\n", ev.t, dir, ev.opcode, ev.payload.size());
        for (size_t i = 0; i < ev.payload.size(); i += 16) {
            fprintf(m_f, "    ");
            for (size_t j = i; j < i + 16 && j < ev.payload.size(); ++j)
                fprintf(m_f, "%02X ", ev.payload[j]);
            fputc('\n', m_f);
        }
        fflush(m_f);
    }
    if (m_echo) printf("[%9.3f] %s 0x%04X size=%zu\n", ev.t, dir, ev.opcode, ev.payload.size());
}

void WireLog::close() {
    if (m_f) { fclose(m_f); m_f = nullptr; }
}

}
