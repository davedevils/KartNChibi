// captures c2s traffic a ws2 hook cannot see and can inject a direct login to bypass the dead launcher token

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include "net/Packet.h"

#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <share.h>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

using knc::Packet;

namespace {

struct Config {
    std::string listenIp   = "127.0.0.1";
    uint16_t    listenPort = 50017;
    std::string targetHost = "203.0.113.10";
    uint16_t    targetPort = 50017;
    bool        inject     = true;   // plant direct login and drop client login family
    std::string version    = "1";
    std::string user       = "changeme";
    std::string pass       = "changeme";
    std::string logPath    = "mitm_packets.log";
    int         maxHex     = 256;
    bool        heartbeat  = false;
};

Config g_cfg;
FILE*  g_log = nullptr;
std::mutex g_logMu;

// only the login connection injects a room or race opens a second connection that must pass through untouched
std::atomic<bool> g_haveLoginConn{false};

constexpr size_t kHeader   = 8;
// wire length is a u16 so frames can reach header plus 65535 a smaller cap would desync mid frame
constexpr size_t kMaxFrame = kHeader + 0xFFFF;

const char* opcodeName(uint16_t op) {
    switch (op) {
        case 0x0002: return "DISPLAY_MSG";
        case 0x0007: return "CLIENT_AUTH/PLAYER_INFO";
        case 0x000E: return "CHANNEL_LIST";
        case 0x0011: return "MENU";
        case 0x0012: return "LOBBY";
        case 0x0013: return "ROOM_CONTEXT";
        case 0x0021: return "ROOM_MEMBER";
        case 0x0022: return "ROOM_LEAVE";
        case 0x002D: return "LOBBY_ROOM_ADD/CREATE_ROOM";
        case 0x0030: return "ROOM_STATE";
        case 0x0031: return "POSITION";
        case 0x0032: return "SLOT_ENABLED";
        case 0x0033: return "READY_STATE";
        case 0x0034: return "ALL_START";
        case 0x003A: return "RACE_GO";
        case 0x003C: return "FINISH";
        case 0x003E: return "GRID_RACER";
        case 0x003F: return "JOIN_ROOM";
        case 0x0040: return "MOTION";
        case 0x0041: return "CHECKPOINT";
        case 0x0054: return "GAME_REDIRECT";
        case 0x00D0: return "CLIENT_INFO";
        case 0x00FA: return "FULL_STATE";
        case 0x00FE: return "LAUNCHER_LOGIN";
        default:     return "?";
    }
}

bool isHeartbeat(uint16_t op) { return op == 0x0001 || op == 0x000B; }
bool isLoginFamily(uint16_t op) {
    return op == 0x0007 || op == 0x00FA || op == 0x00D0 || op == 0x00FE;
}

std::u16string toU16(const std::string& s) {
    std::u16string o;
    o.reserve(s.size());
    for (unsigned char c : s) o.push_back(static_cast<char16_t>(c));
    return o;
}

void stamp(char* out, size_t n) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    _snprintf_s(out, n, _TRUNCATE, "%02u:%02u:%02u.%03u",
                st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
}

void logFrame(const char* dir, const uint8_t* f, size_t total, const char* tag) {
    const uint16_t len = static_cast<uint16_t>(f[0] | (f[1] << 8));
    const uint16_t op  = static_cast<uint16_t>(f[2] | (f[3] << 8));
    if (!g_cfg.heartbeat && isHeartbeat(op)) return;

    std::lock_guard<std::mutex> lk(g_logMu);
    char ts[32];
    stamp(ts, sizeof(ts));
    fprintf(g_log, "[%s] %s op=0x%04X %-24s len=%u total=%zu%s\n",
            ts, dir, op, opcodeName(op), len, total, tag ? tag : "");
    const int cap = (len > static_cast<uint16_t>(g_cfg.maxHex)) ? g_cfg.maxHex : static_cast<int>(len);
    const uint8_t* p = f + kHeader;
    for (int i = 0; i < cap; i += 16) {
        fprintf(g_log, "    %04X  ", i);
        for (int j = 0; j < 16; ++j) {
            if (i + j < cap) fprintf(g_log, "%02X ", p[i + j]);
            else fprintf(g_log, "   ");
        }
        fputc(' ', g_log);
        for (int j = 0; j < 16 && i + j < cap; ++j) {
            const uint8_t c = p[i + j];
            fputc((c >= 0x20 && c < 0x7F) ? c : '.', g_log);
        }
        fputc('\n', g_log);
    }
    if (cap < static_cast<int>(len)) fprintf(g_log, "    ... %u more bytes\n", len - cap);
    fflush(g_log);
}

void logLine(const char* fmt, ...) {
    std::lock_guard<std::mutex> lk(g_logMu);
    char ts[32];
    stamp(ts, sizeof(ts));
    fprintf(g_log, "[%s] ", ts);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fputc('\n', g_log);
    fflush(g_log);
}

bool sendAll(SOCKET s, const uint8_t* data, size_t len) {
    size_t off = 0;
    while (off < len) {
        int n = send(s, reinterpret_cast<const char*>(data) + off,
                     static_cast<int>(len - off), 0);
        if (n <= 0) return false;
        off += static_cast<size_t>(n);
    }
    return true;
}

void injectDirectLogin(SOCKET upstream) {
    Packet fs(0x00FA);
    auto a = fs.serialize();
    sendAll(upstream, a.data(), a.size());

    Packet auth(0x0007);
    auth.writeString(g_cfg.version);
    auth.writeInt32(4);
    auth.writeWString(toU16(g_cfg.user));
    auth.writeWString(toU16(g_cfg.pass));
    auto b = auth.serialize();
    sendAll(upstream, b.data(), b.size());

    logLine("INJECT direct login user=%s version=%s action=4 (0xFA + 0x07)",
            g_cfg.user.c_str(), g_cfg.version.c_str());
}

// reassembles the stream into frames logs each forwards or drops and signals a dead socket to the caller
void pump(SOCKET from, SOCKET to, const char* dir, bool c2s, std::atomic<bool>& alive,
          std::vector<uint8_t> buf, bool allowDrop = true,
          std::atomic<bool>* loginPhase = nullptr) {
    char chunk[16384];
    for (;;) {
        // drain first so a seeded first frame is processed before any recv
        size_t off = 0;
        while (buf.size() - off >= kHeader) {
            const uint16_t plen = static_cast<uint16_t>(buf[off] | (buf[off + 1] << 8));
            const size_t total = kHeader + plen;
            if (total > kMaxFrame) {
                logLine("%s DESYNC len=%u drop %zu bytes", dir, plen, buf.size() - off);
                buf.clear();
                off = 0;
                break;
            }
            if (buf.size() - off < total) break;

            const uint8_t* frame = buf.data() + off;
            const uint16_t op = static_cast<uint16_t>(frame[2] | (frame[3] << 8));
            // drops the login family only while login is in flight since those opcodes carry state and cash later
            const bool inLogin = !loginPhase || loginPhase->load();
            const bool drop = c2s && g_cfg.inject && allowDrop && inLogin && isLoginFamily(op);
            logFrame(dir, frame, total, drop ? "  DROPPED client login family" : nullptr);
            if (!drop) {
                if (!sendAll(to, frame, total)) { alive.store(false); break; }
            }
            // the accept ends the login phase everything passes through after it
            if (!c2s && loginPhase && op == 0x0007) loginPhase->store(false);
            off += total;
        }
        if (off) buf.erase(buf.begin(), buf.begin() + off);

        if (!alive.load()) break;
        int n = recv(from, chunk, sizeof(chunk), 0);
        if (n <= 0) break;
        buf.insert(buf.end(), reinterpret_cast<uint8_t*>(chunk),
                   reinterpret_cast<uint8_t*>(chunk) + n);
    }
    alive.store(false);
}

// reads until one whole frame is buffered keeping extra bytes to tell a launcher login from a game connection
bool readFirstFrame(SOCKET s, std::vector<uint8_t>& buf, uint16_t& op) {
    char chunk[16384];
    for (;;) {
        if (buf.size() >= kHeader) {
            const uint16_t plen = static_cast<uint16_t>(buf[0] | (buf[1] << 8));
            const size_t total = kHeader + plen;
            if (total > kMaxFrame) return false;
            if (buf.size() >= total) { op = static_cast<uint16_t>(buf[2] | (buf[3] << 8)); return true; }
        }
        int n = recv(s, chunk, sizeof(chunk), 0);
        if (n <= 0) return false;
        buf.insert(buf.end(), reinterpret_cast<uint8_t*>(chunk),
                   reinterpret_cast<uint8_t*>(chunk) + n);
    }
}

// answers 0xFE locally since the launcher only needs a success flag and a token the foreign login stays clean
void answerLauncherLogin(SOCKET client) {
    std::vector<uint8_t> p;
    p.push_back(0x01);                                 // success
    for (const char* t = "mitm"; *t; ++t) p.push_back(static_cast<uint8_t>(*t));
    p.push_back(0);
    for (const char* m = "ok"; *m; ++m) p.push_back(static_cast<uint8_t>(*m));
    p.push_back(0);
    std::vector<uint8_t> f;
    const uint16_t sz = static_cast<uint16_t>(p.size());
    f.push_back(sz & 0xFF); f.push_back((sz >> 8) & 0xFF);
    f.push_back(0xFE); f.push_back(0);
    f.push_back(0); f.push_back(0); f.push_back(0); f.push_back(0);
    f.insert(f.end(), p.begin(), p.end());
    sendAll(client, f.data(), f.size());
}

SOCKET connectUpstream() {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    char portStr[16];
    _snprintf_s(portStr, sizeof(portStr), _TRUNCATE, "%u", g_cfg.targetPort);
    addrinfo* res = nullptr;
    if (getaddrinfo(g_cfg.targetHost.c_str(), portStr, &hints, &res) != 0 || !res)
        return INVALID_SOCKET;
    SOCKET s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == INVALID_SOCKET) { freeaddrinfo(res); return INVALID_SOCKET; }
    if (connect(s, res->ai_addr, static_cast<int>(res->ai_addrlen)) != 0) {
        closesocket(s);
        freeaddrinfo(res);
        return INVALID_SOCKET;
    }
    freeaddrinfo(res);
    return s;
}

void handleClient(SOCKET client) {
    // a second connection opens during login it is wired straight through with no blocking or injection
    if (g_cfg.inject && g_haveLoginConn.load()) {
        SOCKET server = connectUpstream();
        if (server == INVALID_SOCKET) {
            logLine("game upstream connect to %s:%u FAILED", g_cfg.targetHost.c_str(), g_cfg.targetPort);
            closesocket(client);
            return;
        }
        logLine("game connection up, pass through no inject %s:%u",
                g_cfg.targetHost.c_str(), g_cfg.targetPort);
        std::atomic<bool> alive{true};
        std::thread up([&] { pump(client, server, "C2S-game", true, alive, {}, false); });
        pump(server, client, "S2C-game", false, alive, {}, false);
        alive.store(false);
        shutdown(client, SD_BOTH);
        shutdown(server, SD_BOTH);
        up.join();
        closesocket(client);
        closesocket(server);
        logLine("game connection closed");
        return;
    }

    // peeks the first frame a launcher validation is 0xFE then close a game connection opens with 0xFA
    std::vector<uint8_t> head;
    uint16_t firstOp = 0;
    if (!readFirstFrame(client, head, firstOp)) { closesocket(client); return; }

    if (g_cfg.inject && firstOp == 0x00FE) {
        const size_t total = kHeader + (head[0] | (head[1] << 8));
        logFrame("C2S", head.data(), total, "  launcher login faked locally");
        answerLauncherLogin(client);
        logLine("launcher login answered locally token issued");
        Sleep(200);
        shutdown(client, SD_BOTH);
        closesocket(client);
        return;
    }

    SOCKET server = connectUpstream();
    if (server == INVALID_SOCKET) {
        logLine("upstream connect to %s:%u FAILED", g_cfg.targetHost.c_str(), g_cfg.targetPort);
        closesocket(client);
        return;
    }
    logLine("client up, upstream %s:%u connected", g_cfg.targetHost.c_str(), g_cfg.targetPort);
    if (g_cfg.inject) { injectDirectLogin(server); g_haveLoginConn.store(true); }

    std::atomic<bool> alive{true};
    std::atomic<bool> loginPhase{true};   // drop login family only until the accept
    std::thread up([&] { pump(client, server, "C2S", true, alive, head, true, &loginPhase); });
    pump(server, client, "S2C", false, alive, {}, true, &loginPhase);
    alive.store(false);
    g_haveLoginConn.store(false);
    shutdown(client, SD_BOTH);
    shutdown(server, SD_BOTH);
    up.join();
    closesocket(client);
    closesocket(server);
    logLine("session closed");
}

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    return s.substr(a, b - a + 1);
}

void loadConfig(const char* path) {
    FILE* f = nullptr;
    fopen_s(&f, path, "r");
    if (!f) return;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        std::string s = trim(line);
        if (s.empty() || s[0] == '#' || s[0] == ';') continue;
        size_t eq = s.find('=');
        if (eq == std::string::npos) continue;
        std::string k = trim(s.substr(0, eq));
        std::string v = trim(s.substr(eq + 1));
        if      (k == "listen_ip")   g_cfg.listenIp   = v;
        else if (k == "listen_port") g_cfg.listenPort = static_cast<uint16_t>(atoi(v.c_str()));
        else if (k == "target_host") g_cfg.targetHost = v;
        else if (k == "target_port") g_cfg.targetPort = static_cast<uint16_t>(atoi(v.c_str()));
        else if (k == "mode")        g_cfg.inject     = (v == "inject");
        else if (k == "version")     g_cfg.version    = v;
        else if (k == "user")        g_cfg.user       = v;
        else if (k == "pass")        g_cfg.pass       = v;
        else if (k == "log")         g_cfg.logPath    = v;
        else if (k == "maxhex")      g_cfg.maxHex     = atoi(v.c_str());
        else if (k == "heartbeat")   g_cfg.heartbeat  = (atoi(v.c_str()) != 0);
    }
    fclose(f);
}

}  // namespace

int main(int argc, char** argv) {
    loadConfig(argc > 1 ? argv[1] : "mitm.ini");
    loadConfig("mitm.local.ini");  // git ignored real values override the placeholders above

    // share deny none so the log can be tailed live while the proxy runs
    g_log = _fsopen(g_cfg.logPath.c_str(), "w", _SH_DENYNO);
    if (!g_log) g_log = stderr;

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }

    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) { fprintf(stderr, "socket failed\n"); return 1; }
    BOOL yes = TRUE;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&yes), sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(g_cfg.listenPort);
    inet_pton(AF_INET, g_cfg.listenIp.c_str(), &addr.sin_addr);
    if (bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        const int err = WSAGetLastError();
        fprintf(stderr, "bind %s:%u failed err=%d\n",
                g_cfg.listenIp.c_str(), g_cfg.listenPort, err);
        // 10013 here is usually hyper v winnat holding the port range which can shift after a reboot
        if (err == WSAEACCES) {
            fprintf(stderr,
                "  WSAEACCES means Windows reserved this port, not that it is busy.\n"
                "  list the reserved ranges with\n"
                "    netsh interface ipv4 show excludedportrange protocol=tcp\n"
                "  then pick a port in a gap and set listen_port in mitm ini plus the\n"
                "  client network ini and launcher ini to match.\n");
        } else if (err == WSAEADDRINUSE) {
            fprintf(stderr, "  another process already listens there, our own login server maybe\n");
        }
        return 1;
    }
    listen(listener, 8);

    printf("KnC MITM listening %s:%u  ->  %s:%u   mode=%s  log=%s\n",
           g_cfg.listenIp.c_str(), g_cfg.listenPort,
           g_cfg.targetHost.c_str(), g_cfg.targetPort,
           g_cfg.inject ? "inject" : "passthrough", g_cfg.logPath.c_str());
    if (g_cfg.inject)
        printf("inject login user=%s (client login family will be dropped)\n", g_cfg.user.c_str());
    logLine("MITM up %s:%u -> %s:%u mode=%s", g_cfg.listenIp.c_str(), g_cfg.listenPort,
            g_cfg.targetHost.c_str(), g_cfg.targetPort, g_cfg.inject ? "inject" : "passthrough");

    for (;;) {
        sockaddr_in ca{};
        int cl = sizeof(ca);
        SOCKET client = accept(listener, reinterpret_cast<sockaddr*>(&ca), &cl);
        if (client == INVALID_SOCKET) continue;
        std::thread(handleClient, client).detach();
    }
}
