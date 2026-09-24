
#include "spy.h"

#include <winsock2.h>
#include <windows.h>

#include <cstdio>
#include <share.h>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace spy {
namespace {

Config g_cfg;
FILE* g_log = nullptr;
FILE* g_diag = nullptr;   // clientpatch log written in both modes

// a hooked prototype can crash inside the client so never print raw client pointers
const char* okStr(const char* p) {
    if (!p) return "(null)";
    __try {
        for (int i = 0; i < 256; ++i) {
            const unsigned char c = static_cast<unsigned char>(p[i]);
            if (c == 0) return i > 0 ? p : "(empty)";
            if (c < 9 || c > 126) return "(binary)";
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return "(unreadable)";
    }
    return "(unterminated)";
}

std::mutex g_mu;

using send_t = int(WSAAPI*)(SOCKET, const char*, int, int);
using recv_t = int(WSAAPI*)(SOCKET, char*, int, int);
using connect_t = int(WSAAPI*)(SOCKET, const sockaddr*, int);

send_t g_realSend = nullptr;
recv_t g_realRecv = nullptr;
connect_t g_realConnect = nullptr;

// this client never calls send or recv  it uses the WSA overlapped pair
using wsasend_t = int(WSAAPI*)(SOCKET, LPWSABUF, DWORD, LPDWORD, DWORD,
                               LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);
using wsarecv_t = int(WSAAPI*)(SOCKET, LPWSABUF, DWORD, LPDWORD, LPDWORD,
                               LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);
wsasend_t g_realWsaSend = nullptr;
wsarecv_t g_realWsaRecv = nullptr;

// five byte relative jmp is the only patch shape we install
struct Trampoline {
    void* target = nullptr;
    uint8_t saved[5] = {};
    bool armed = false;
};
Trampoline g_sendTr, g_recvTr, g_wsaSendTr, g_wsaRecvTr, g_connectTr;

// stream reassembly one buffer per direction per socket
struct Stream {
    std::vector<uint8_t> in;
    std::vector<uint8_t> out;
};
std::map<SOCKET, Stream> g_streams;

// one read in flight per connection tracked separately for login and world connections
constexpr int kMaxConns = 8;
uintptr_t g_connId[kMaxConns] = {};
int       g_connInFlight[kMaxConns] = {};

int* inFlightSlot(uintptr_t self) {
    for (int i = 0; i < kMaxConns; ++i) if (g_connId[i] == self) return &g_connInFlight[i];
    for (int i = 0; i < kMaxConns; ++i) if (g_connId[i] == 0) { g_connId[i] = self; return &g_connInFlight[i]; }
    return nullptr;
}

constexpr size_t kHeader = 8;
constexpr size_t kMaxFrame = 0x2000;

const char* opcodeName(uint16_t op) {
    switch (op) {
        case 0x0002: return "DISPLAY_MSG";
        case 0x0003: return "CHARCREATE_OPEN";
        case 0x0004: return "REGISTER_NICK";
        case 0x0007: return "PLAYER_INFO";
        case 0x000A: return "CONNECTION_OK";
        case 0x000C: return "PING_ICONS";
        case 0x000D: return "RANK_BOARD";
        case 0x000E: return "CHANNEL_LIST";
        case 0x000F: return "GARAGE";
        case 0x0010: return "SHOP";
        case 0x0011: return "MENU";
        case 0x0012: return "LOBBY";
        case 0x0013: return "ROOM_CONTEXT";
        case 0x0014: return "RACE_INIT";
        case 0x0016: return "LICENSE";
        case 0x001B: return "CHARACTER_LIST";
        case 0x001C: return "KART_LIST";
        case 0x001D: return "ACCESSORY_LIST";
        case 0x001E: return "ACCESSORY_LIST2";
        case 0x0021: return "ROOM_MEMBER";
        case 0x0022: return "ROOM_LEAVE";
        case 0x0030: return "ROOM_STATE";
        case 0x0031: return "POSITION";
        case 0x0032: return "SLOT_ENABLED";
        case 0x0033: return "READY_STATE";
        case 0x0034: return "ALL_START";
        case 0x003A: return "RACE_GO";
        case 0x003C: return "FINISH";
        case 0x003D: return "RANK";
        case 0x003E: return "GRID_RACER";
        case 0x003F: return "ROOM_INFO";
        case 0x0040: return "MOTION";
        case 0x0046: return "SCOREBOARD";
        case 0x0058: return "FINISH_ECHO";
        case 0x0063: return "CREATE_ROOM_ACK";
        case 0x0068: return "TELEPORT";
        case 0x0087: return "MISSION_DEFS";
        case 0x0088: return "MISSION_PROGRESS";
        case 0x008F: return "MISSION_MENU";
        case 0x00A2: return "LICENSE_PROGRESS";
        case 0x00BE: return "CATALOG_CLEAR";
        case 0x00BF: return "CATALOG_CHARACTER";
        case 0x00C0: return "CATALOG_KART";
        case 0x00C1: return "CATALOG_ACCESSORY";
        case 0x00C2: return "CATALOG_SKIN";
        case 0x00C6: return "PRICE_TABLE";
        case 0x0103: return "DEF_TABLE_103";
        case 0x0104: return "INVENTORY";
        case 0x0107: return "CUSTOMCAR_PRESETS";
        case 0x0108: return "PART_NAMES";
        case 0x0109: return "PART_INSTANCE";
        case 0x010A: return "CARCRAFT";
        case 0x010C: return "ROOMCRAFT_DEFS";
        case 0x010D: return "ROOMCRAFT_PLACED";
        case 0x010E: return "ROOMCRAFT_STAGE";
        default: return "?";
    }
}

bool isHeartbeat(uint16_t op) { return op == 0x0001 || op == 0x000B; }

void stamp(char* out, size_t n) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    _snprintf_s(out, n, _TRUNCATE, "%02u:%02u:%02u.%03u",
                st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
}

void logFrame(const char* dir, SOCKET s, const uint8_t* frame, size_t total) {
    const uint16_t len = static_cast<uint16_t>(frame[0] | (frame[1] << 8));
    const uint16_t op = static_cast<uint16_t>(frame[2] | (frame[3] << 8));

    if (!g_cfg.logHeartbeat && isHeartbeat(op)) return;

    char ts[32];
    stamp(ts, sizeof(ts));

    fprintf(g_log, "[%s] %s sock=%llu op=0x%04X %-18s len=%u total=%zu\n",
            ts, dir, static_cast<unsigned long long>(s), op, opcodeName(op), len, total);

    if (g_cfg.hexDump && len > 0) {
        const uint8_t* p = frame + kHeader;
        const int n = (len > static_cast<uint16_t>(g_cfg.maxHexBytes))
                          ? g_cfg.maxHexBytes : static_cast<int>(len);
        for (int i = 0; i < n; i += 16) {
            fprintf(g_log, "    %04X  ", i);
            for (int j = 0; j < 16; ++j) {
                if (i + j < n) fprintf(g_log, "%02X ", p[i + j]);
                else fprintf(g_log, "   ");
            }
            fputc(' ', g_log);
            for (int j = 0; j < 16 && i + j < n; ++j) {
                const uint8_t c = p[i + j];
                fputc((c >= 0x20 && c < 0x7F) ? c : '.', g_log);
            }
            fputc('\n', g_log);
        }
        if (n < static_cast<int>(len)) fprintf(g_log, "    ... %u more bytes\n", len - n);
    }
    fflush(g_log);
}

// pull every complete frame out of the accumulated stream
void consume(const char* dir, SOCKET s, std::vector<uint8_t>& buf,
             const char* data, int len) {
    if (len <= 0) return;
    buf.insert(buf.end(), reinterpret_cast<const uint8_t*>(data),
               reinterpret_cast<const uint8_t*>(data) + len);

    size_t off = 0;
    while (buf.size() - off >= kHeader) {
        const uint16_t plen = static_cast<uint16_t>(buf[off] | (buf[off + 1] << 8));
        const size_t total = kHeader + plen;
        if (total > kMaxFrame) {
            // desync or not our protocol drop the buffer rather than log garbage
            char ts[32];
            stamp(ts, sizeof(ts));
            fprintf(g_log, "[%s] %s DESYNC len=%u dropping %zu bytes\n",
                    ts, dir, plen, buf.size() - off);
            fflush(g_log);
            buf.clear();
            return;
        }
        if (buf.size() - off < total) break;  // partial frame wait for more
        logFrame(dir, s, buf.data() + off, total);
        off += total;
    }
    if (off) buf.erase(buf.begin(), buf.begin() + off);
}

int WSAAPI hookConnect(SOCKET s, const sockaddr* name, int namelen) {
    // logs every connect target to show where redirects really go
    if (g_log && name && name->sa_family == AF_INET) {
        auto a = reinterpret_cast<const sockaddr_in*>(name);
        const unsigned long ip = a->sin_addr.S_un.S_addr;
        fprintf(g_log, "[CONNECT] sock=%llu -> %d.%d.%d.%d:%d\n",
                (unsigned long long)s,
                (int)(ip & 0xFF), (int)((ip >> 8) & 0xFF),
                (int)((ip >> 16) & 0xFF), (int)((ip >> 24) & 0xFF), ntohs(a->sin_port));
        fflush(g_log);
    }
    if (g_cfg.redirectIp && name && name->sa_family == AF_INET) {
        auto in = reinterpret_cast<const sockaddr_in*>(name);
        sockaddr_in patched = *in;
        const unsigned long was = patched.sin_addr.S_un.S_addr;
        patched.sin_addr.S_un.S_addr = g_cfg.redirectIp;
        const unsigned short wasPort = ntohs(patched.sin_port);
        if (g_cfg.redirectPort) patched.sin_port = htons(g_cfg.redirectPort);
        if (g_diag) {
            fprintf(g_diag, "[CONN] %d.%d.%d.%d:%d -> %d.%d.%d.%d:%d\n",
                    (int)(was & 0xFF), (int)((was >> 8) & 0xFF),
                    (int)((was >> 16) & 0xFF), (int)((was >> 24) & 0xFF), wasPort,
                    (int)(g_cfg.redirectIp & 0xFF), (int)((g_cfg.redirectIp >> 8) & 0xFF),
                    (int)((g_cfg.redirectIp >> 16) & 0xFF), (int)((g_cfg.redirectIp >> 24) & 0xFF),
                    (int)ntohs(patched.sin_port));
            fflush(g_diag);
        }
        if (g_log) {
            fprintf(g_log, "[CONN] redirect %d.%d.%d.%d:%d -> %d.%d.%d.%d\n",
                    (int)(was & 0xFF), (int)((was >> 8) & 0xFF),
                    (int)((was >> 16) & 0xFF), (int)((was >> 24) & 0xFF),
                    ntohs(patched.sin_port),
                    (int)(g_cfg.redirectIp & 0xFF), (int)((g_cfg.redirectIp >> 8) & 0xFF),
                    (int)((g_cfg.redirectIp >> 16) & 0xFF), (int)((g_cfg.redirectIp >> 24) & 0xFF));
            fflush(g_log);
        }
        return g_realConnect(s, reinterpret_cast<sockaddr*>(&patched), namelen);
    }
    return g_realConnect(s, name, namelen);
}

int WSAAPI hookSend(SOCKET s, const char* data, int len, int flags) {
    const int r = g_realSend(s, data, len, flags);
    if (g_cfg.enabled && r > 0) {
        std::lock_guard<std::mutex> lk(g_mu);
        consume("C2S", s, g_streams[s].out, data, r);
    }
    return r;
}

int WSAAPI hookRecv(SOCKET s, char* data, int len, int flags) {
    const int r = g_realRecv(s, data, len, flags);
    if (g_cfg.enabled && r > 0) {
        std::lock_guard<std::mutex> lk(g_mu);
        consume("S2C", s, g_streams[s].in, data, r);
    }
    return r;
}

int WSAAPI hookWsaSend(SOCKET s, LPWSABUF b, DWORD n, LPDWORD sent, DWORD f,
                       LPWSAOVERLAPPED o, LPWSAOVERLAPPED_COMPLETION_ROUTINE c) {
    const int r = g_realWsaSend(s, b, n, sent, f, o, c);
    if (g_cfg.enabled && r == 0 && sent && *sent > 0 && b && n) {
        std::lock_guard<std::mutex> lk(g_mu);
        DWORD left = *sent;
        for (DWORD i = 0; i < n && left; ++i) {
            const DWORD take = (b[i].len < left) ? b[i].len : left;
            consume("C2S", s, g_streams[s].out, b[i].buf, static_cast<int>(take));
            left -= take;
        }
    }
    return r;
}

int WSAAPI hookWsaRecv(SOCKET s, LPWSABUF b, DWORD n, LPDWORD got, LPDWORD f,
                       LPWSAOVERLAPPED o, LPWSAOVERLAPPED_COMPLETION_ROUTINE c) {
    const int r = g_realWsaRecv(s, b, n, got, f, o, c);
    if (g_cfg.enabled && r == 0 && got && *got > 0 && b && n) {
        std::lock_guard<std::mutex> lk(g_mu);
        DWORD left = *got;
        for (DWORD i = 0; i < n && left; ++i) {
            const DWORD take = (b[i].len < left) ? b[i].len : left;
            consume("S2C", s, g_streams[s].in, b[i].buf, static_cast<int>(take));
            left -= take;
        }
    }
    return r;
}

// client reads with overlapped ReadFile not recv so no ws2 hook can see it
void* g_realParse = nullptr;
Trampoline g_parseTr;

constexpr uintptr_t kParseLoop = 0x00476CC0;
constexpr uintptr_t kNetBuffer = 0x0080A058;
constexpr uintptr_t kNetCount  = 0x0080C058;

// both globals are indexed by the connection so login and world never mix
void onParse(uintptr_t self, int nbytes) {
    if (int* f = inFlightSlot(self)) *f = 0;  // completion arrived so the next arm is legitimate
    if (!g_cfg.enabled || !g_log) return;
    std::lock_guard<std::mutex> lk(g_mu);

    const int prev = *reinterpret_cast<int*>(kNetCount + self);
    const uint8_t* buf = reinterpret_cast<const uint8_t*>(kNetBuffer + self);
    const int total = prev + nbytes;
    if (total <= 0 || total > static_cast<int>(kMaxFrame) * 4) return;

    fprintf(g_log, "[S2C] conn=%08X arrived=%d carried=%d total=%d\n",
            static_cast<unsigned>(self), nbytes, prev, total);
    {
        // raw head so the wire can be diffed against the server log
        const int n = total < 64 ? total : 64;
        fprintf(g_log, "    head ");
        for (int i = 0; i < n; ++i) fprintf(g_log, "%02X ", buf[i]);
        fprintf(g_log, "\n");
        // and the bytes the read actually landed which may differ
        const uint8_t* fresh = buf + prev;
        const int m = nbytes < 32 ? nbytes : 32;
        fprintf(g_log, "    fresh ");
        for (int i = 0; i < m; ++i) fprintf(g_log, "%02X ", fresh[i]);
        fprintf(g_log, "\n");
    }

    size_t off = 0;
    int idx = 0;
    while (off + kHeader <= static_cast<size_t>(total)) {
        const uint16_t len = static_cast<uint16_t>(buf[off] | (buf[off + 1] << 8));
        const uint16_t op = static_cast<uint16_t>(buf[off + 2] | (buf[off + 3] << 8));
        if (len + kHeader >= kMaxFrame) {
            fprintf(g_log, "    frame %d op=0x%04X len=%u OVER 0x2000 client errors out\n",
                    idx, op, len);
            break;
        }
        if (off + kHeader + len > static_cast<size_t>(total)) {
            fprintf(g_log, "    frame %d op=0x%04X len=%u INCOMPLETE reader stops here\n",
                    idx, op, len);
            break;
        }
        fprintf(g_log, "    frame %d op=0x%04X %-18s len=%u\n", idx, op, opcodeName(op), len);
        // full frame hex so the whole catalogue can be diffed byte for byte
        if (g_cfg.hexDump && len > 0) {
            fprintf(g_log, "      raw ");
            for (size_t k = 0; k < len; ++k) fprintf(g_log, "%02X ", buf[off + kHeader + k]);
            fprintf(g_log, "\n");
        }
        off += kHeader + len;
        ++idx;
    }
    if (off != static_cast<size_t>(total)) {
        fprintf(g_log, "    %zu bytes stay buffered\n", static_cast<size_t>(total) - off);
    }
    fflush(g_log);
}

// client bug the redirect zeroes count causing a negative buffered offset clamp before reading
void* g_realArm = nullptr;
Trampoline g_armTr;
constexpr uintptr_t kArmRecv = 0x00476A50;
constexpr uintptr_t kPending = 0x0080A054;

// only one read is ever in flight by design a duplicate arm is the redirect race
int g_skipArm = 0;

// returns 1 when the caller must not issue the read
int onArm(uintptr_t self) {
    int* count = reinterpret_cast<int*>(kNetCount + self);
    const int before = *count;
    // armfix 0 reproduces stock behavior negative count included
    if (g_cfg.armFix && *count < 0) *count = 0;
    int* flight = inFlightSlot(self);
    const int dup = (g_cfg.armFix && flight) ? *flight : 0;
    if (flight && !*flight) *flight = 1;
    // a clamp or dropped duplicate means the redirect race was caught log it
    if (g_diag && (before < 0 || dup)) {
        fprintf(g_diag, "[ARM] count=%d%s%s\n", before,
                before < 0 ? " CLAMPED" : "", dup ? " DUPLICATE dropped" : "");
        fflush(g_diag);
    }
    if (g_log) {
        const void* handle = *reinterpret_cast<void**>(self + 8);
        fprintf(g_log, "[ARM] conn=%08X sock=%p count=%d%s size=%d%s\n",
                static_cast<unsigned>(self), handle, before,
                before < 0 ? " CLAMPED" : "", 0x2000 - *count,
                dup ? " DUPLICATE dropped" : "");
        fflush(g_log);
    }
    return dup;
}

// sub 490A70 attaches the driver body its failure is the set body fail box
void* g_realSetBody = nullptr;
Trampoline g_setBodyTr;
constexpr uintptr_t kSetBody = 0x00490A70;

void dumpBlock(const char* tag, const uint8_t* p, int n) {
    if (!p) { fprintf(g_log, "    %s NULL\n", tag); return; }
    fprintf(g_log, "    %s ", tag);
    for (int i = 0; i < n; ++i) fprintf(g_log, "%02X ", p[i]);
    fprintf(g_log, "\n");
}

void onSetBody(uintptr_t self, uint32_t car, uint32_t driver,
               const uint8_t* a3, const uint8_t* a4) {
    if (!g_log) return;
    fprintf(g_log, "[BODY] this=%08X car=%08X driver=%08X a3=%p a4=%p\n",
            static_cast<unsigned>(self), car, driver,
            static_cast<const void*>(a3), static_cast<const void*>(a4));
    dumpBlock("a3", a3, 64);
    dumpBlock("a4", a4, 64);
    fflush(g_log);
}

// model loaders build paths with sprintf a missing asset is why a stage gives up
using createfilea_t = HANDLE(WINAPI*)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES,
                                      DWORD, DWORD, HANDLE);
using createfilew_t = HANDLE(WINAPI*)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES,
                                      DWORD, DWORD, HANDLE);
createfilea_t g_realCreateFileA = nullptr;
createfilew_t g_realCreateFileW = nullptr;
Trampoline g_cfaTr, g_cfwTr;

// client reads a stale registry path and resets cwd undoing the DllMain fix
using setcwda_t = BOOL(WINAPI*)(LPCSTR);
using setcwdw_t = BOOL(WINAPI*)(LPCWSTR);
setcwda_t g_realSetCwdA = nullptr;
setcwdw_t g_realSetCwdW = nullptr;
Trampoline g_scdaTr, g_scdwTr;
char g_ourDir[MAX_PATH] = {};

// case insensitive substring no locale marks the foreign install to dodge
bool ciHas(const char* hay, const char* needle) {
    if (!hay) return false;
    for (const char* h = hay; *h; ++h) {
        const char* a = h;
        const char* b = needle;
        while (*b && *a && ((*a | 32) == (*b | 32))) { ++a; ++b; }
        if (!*b) return true;
    }
    return false;
}

// anywhere outside our own folder counts as foreign since the client should never leave it
bool underOurDir(const char* p) {
    if (!p || !*p || !g_ourDir[0]) return false;
    char full[MAX_PATH] = {};
    if (!GetFullPathNameA(p, MAX_PATH, full, nullptr)) return false;
    const size_t n = strlen(g_ourDir);
    for (size_t i = 0; i < n; ++i) {
        const char a = full[i] == '/' ? '\\' : full[i];
        const char b = g_ourDir[i] == '/' ? '\\' : g_ourDir[i];
        if ((a | 32) != (b | 32)) return false;
    }
    return full[n] == 0 || full[n] == '\\' || full[n] == '/';
}

bool foreignDir(const char* p) { return !underOurDir(p); }

BOOL WINAPI hookSetCwdA(LPCSTR path) {
    if (g_ourDir[0] && foreignDir(path)) {
        if (g_diag) { fprintf(g_diag, "[CWD] foreign '%s' -> %s\n", path ? path : "(null)", g_ourDir); fflush(g_diag); }
        return g_realSetCwdA(g_ourDir);
    }
    const BOOL r = g_realSetCwdA(path);
    if (!r && g_ourDir[0]) {
        if (g_diag) { fprintf(g_diag, "[CWD] refused '%s' -> %s\n", path ? path : "(null)", g_ourDir); fflush(g_diag); }
        return g_realSetCwdA(g_ourDir);
    }
    return r;
}
BOOL WINAPI hookSetCwdW(LPCWSTR path) {
    char narrow[MAX_PATH] = {};
    if (path) WideCharToMultiByte(CP_ACP, 0, path, -1, narrow, MAX_PATH, nullptr, nullptr);
    if (g_ourDir[0] && foreignDir(narrow)) {
        if (g_diag) { fprintf(g_diag, "[CWD] foreign wide '%s' -> %s\n", narrow, g_ourDir); fflush(g_diag); }
        return g_realSetCwdA(g_ourDir);
    }
    const BOOL r = g_realSetCwdW(path);
    if (!r && g_ourDir[0]) {
        if (g_diag) { fprintf(g_diag, "[CWD] refused wide -> %s\n", g_ourDir); fflush(g_diag); }
        return g_realSetCwdA(g_ourDir);
    }
    return r;
}

// model loads show whether the driver and kart were even requested
bool modelPath(const char* p) {
    if (!p) return false;
    static const char* keys[] = { "Driver", "FactoryCar", ".car", ".nif", "BODYSET" };
    for (const char* k : keys) {
        for (const char* q = p; *q; ++q) {
            const char* a = q;
            const char* b = k;
            while (*b && *a && ((*a | 32) == (*b | 32))) { ++a; ++b; }
            if (!*b) return true;
        }
    }
    return false;
}

bool interestingPath(const char* p) {
    if (!p) return false;
    // only game content skip the flood of system and font handles
    for (const char* q = p; *q; ++q) {
        if ((q[0] == 'D' || q[0] == 'd') && (q[1] == 'a' || q[1] == 'A') &&
            (q[2] == 't' || q[2] == 'T') && (q[3] == 'a' || q[3] == 'A')) return true;
    }
    return false;
}

HANDLE WINAPI hookCreateFileA(LPCSTR name, DWORD acc, DWORD share,
                              LPSECURITY_ATTRIBUTES sa, DWORD disp, DWORD flags, HANDLE tmpl) {
    const HANDLE h = g_realCreateFileA(name, acc, share, sa, disp, flags, tmpl);
    if (g_log && name) {
        const bool model = modelPath(name);
        if (model || (h == INVALID_HANDLE_VALUE && interestingPath(name))) {
            fprintf(g_log, "[%s] %s\n",
                    h == INVALID_HANDLE_VALUE ? "MISS" : "OPEN", name);
            fflush(g_log);
        }
    }
    return h;
}

HANDLE WINAPI hookCreateFileW(LPCWSTR name, DWORD acc, DWORD share,
                              LPSECURITY_ATTRIBUTES sa, DWORD disp, DWORD flags, HANDLE tmpl) {
    const HANDLE h = g_realCreateFileW(name, acc, share, sa, disp, flags, tmpl);
    if (g_log && h == INVALID_HANDLE_VALUE && name) {
        char buf[512] = {};
        WideCharToMultiByte(CP_ACP, 0, name, -1, buf, sizeof(buf) - 1, nullptr, nullptr);
        if (interestingPath(buf)) {
            fprintf(g_log, "[MISS] %s\n", buf);
            fflush(g_log);
        }
    }
    return h;
}

// bisects licence init via sub 437190 sub 44BF30 sub 4A5E00 last logged call is the one that failed
void* g_realGradeBtn = nullptr;
void* g_realViewport = nullptr;
void* g_realBuildModel = nullptr;
Trampoline g_gradeTr, g_viewTr, g_buildTr;
constexpr uintptr_t kGradeBtn   = 0x0044BF30;
constexpr uintptr_t kViewport   = 0x004A5E00;
constexpr uintptr_t kBuildModel = 0x004A5ED0;

void probeGrade() { if (g_log) { fprintf(g_log, "[STEP] gradeButton\n"); fflush(g_log); } }
void probeView()  { if (g_log) { fprintf(g_log, "[STEP] viewport\n");    fflush(g_log); } }
void probeBuildArgs(const uint32_t* charDef, const uint32_t* kartDef) {
    if (!g_log) return;
    fprintf(g_log, "[STEP] buildModel charDef=%p kartDef=%p\n",
            static_cast<const void*>(charDef), static_cast<const void*>(kartDef));
    if (kartDef) {
        fprintf(g_log, "       kart id=%u custom=%u partKeys=%u,%u,%u,%u,%u,%u name=%s\n",
                kartDef[2], kartDef[5],
                kartDef[33], kartDef[34], kartDef[35],
                kartDef[36], kartDef[37], kartDef[38],
                okStr(reinterpret_cast<const char*>(kartDef) + 32));
    }
    if (charDef) {
        fprintf(g_log, "       char id=%u key=%u name=%s\n",
                charDef[0], charDef[3],
                okStr(reinterpret_cast<const char*>(charDef) + 20));
    }
    fflush(g_log);
}

// sub 43DC20 render entry gated here logs only on change to avoid flooding
void* g_realSceneRender = nullptr;
Trampoline g_sceneTr;
constexpr uintptr_t kSceneRender = 0x0043DC20;

void probeSceneRender(uintptr_t device, uintptr_t sink) {
    if (!g_log) return;
    static uintptr_t lastDev = 0, lastSink = 0, lastCam = 0;
    const uintptr_t cam  = *reinterpret_cast<uintptr_t*>(device + 0x8C);
    const uint8_t   gate = *reinterpret_cast<uint8_t*>(device + 0xA8);
    if (device == lastDev && sink == lastSink && cam == lastCam) return;
    lastDev = device; lastSink = sink; lastCam = cam;
    fprintf(g_log, "[SCENE] device=%08X gate=%u sink=%08X camera=%08X",
            static_cast<unsigned>(device), gate,
            static_cast<unsigned>(sink), static_cast<unsigned>(cam));
    if (sink) {
        const uint32_t* w = reinterpret_cast<const uint32_t*>(sink);
        fprintf(g_log, " sink[0..3]=%08X %08X %08X %08X", w[0], w[1], w[2], w[3]);
    }
    fputc('\n', g_log);
    fflush(g_log);
}

__declspec(naked) void hookSceneRender() {
    __asm { pushad
            pushfd
            mov  eax, [esp + 0x28]
            push eax
            mov  eax, [esp + 0x20]
            push eax
            call probeSceneRender
            add  esp, 8
            popfd
            popad
            jmp dword ptr [g_realSceneRender] }
}

__declspec(naked) void hookGradeBtn() {
    __asm { pushad
            pushfd
            call probeGrade
            popfd
            popad
            jmp dword ptr [g_realGradeBtn] }
}
__declspec(naked) void hookViewport() {
    __asm { pushad
            pushfd
            call probeView
            popfd
            popad
            jmp dword ptr [g_realViewport] }
}
// the model loader the builder calls its two arguments are the built paths
void* g_realLoadModel = nullptr;
void* g_realFitModel = nullptr;
Trampoline g_loadTr, g_fitTr;
constexpr uintptr_t kLoadModel = 0x00444A20;
constexpr uintptr_t kFitModel  = 0x00498DE0;

void probeLoad(const char* modelPath, const char* texPath) {
    if (!g_log) return;
    fprintf(g_log, "[LOAD] model=%s tex=%s\n",
            okStr(modelPath), okStr(texPath));
    fflush(g_log);
}
void probeFit() { if (g_log) { fprintf(g_log, "[FIT] bounds step\n"); fflush(g_log); } }

// the driver body uses its own loader pair not sub 444A20
void* g_realDrvLoad = nullptr;
void* g_realDrvBind = nullptr;
Trampoline g_drvLoadTr, g_drvBindTr;
constexpr uintptr_t kDriverModel = 0x00443F50;
constexpr uintptr_t kDriverBind  = 0x00443DF0;

void probeDrvLoad(const char* a, const char* b) {
    if (!g_log) return;
    fprintf(g_log, "[DRV ] load a=%s b=%s\n", okStr(a), okStr(b));
    fflush(g_log);
}

// sub 490A70 has many branches to one shared fail label log the last lookup key before it
void* g_realPartLk = nullptr;
void* g_realItemLk = nullptr;
Trampoline g_partLkTr, g_itemLkTr;
constexpr uintptr_t kPartLookup = 0x004510C0;
constexpr uintptr_t kItemLookup = 0x004508C0;

void probePartLk(uint32_t container, uint32_t key) {
    if (!g_log) return;
    fprintf(g_log, "[LKUP] part container=%08X key=%u\n", container, key);
    fflush(g_log);
}
void probeItemLk(uint32_t container, uint32_t key) {
    if (!g_log) return;
    fprintf(g_log, "[LKUP] item container=%08X key=%u\n", container, key);
    fflush(g_log);
}


// sub 43D730 is the stage init box the return address names the caller
void* g_realMsgBox = nullptr;
Trampoline g_msgTr;
constexpr uintptr_t kMsgBox = 0x0043D730;

void probeMsgBox(uint32_t retAddr, uint32_t ecx, uint32_t arg0) {
    if (!g_log) return;
    fprintf(g_log, "[MSG ] box from caller %08X ecx=%08X arg0=%08X text=%s\n",
            retAddr, ecx, arg0,
            arg0 > 0x400000 && arg0 < 0x700000 ? reinterpret_cast<const char*>(arg0) : "?");
    fflush(g_log);
}

// the room boom trace the client names its own give up path
void* g_realShowMsg = nullptr;   Trampoline g_showMsgTr;
void* g_realNetConn = nullptr;   Trampoline g_netConnTr;
void* g_realNetDisc = nullptr;   Trampoline g_netDiscTr;
void* g_realNetState = nullptr;  Trampoline g_netStateTr;
constexpr uintptr_t kShowMsg  = 0x004641E0;  // thiscall key flag netconn ip port
constexpr uintptr_t kNetConn  = 0x004774C0;
constexpr uintptr_t kNetDisc  = 0x004775C0;  // thiscall showmsg netstate reconnect state
constexpr uintptr_t kNetState = 0x00426E60;

const char* safeStr(uint32_t p) {
    return p > 0x400000 && p < 0x02F33000 ? reinterpret_cast<const char*>(p) : "?";
}

void probeShowMsg(uint32_t ret, uint32_t ecx, uint32_t key, uint32_t flag) {
    if (!g_log) return;
    fprintf(g_log, "[NETMSG] %s flag=%u from %08X this=%08X\n",
            safeStr(key), flag, ret, ecx);
    fflush(g_log);
}

void probeNetConn(uint32_t ret, uint32_t ip, uint32_t port) {
    if (!g_log) return;
    fprintf(g_log, "[NETCONN] connect %s port %u from %08X\n", safeStr(ip), port, ret);
    fflush(g_log);
}

void probeNetDisc(uint32_t ret, uint32_t ecx, uint32_t showMsg) {
    if (!g_log) return;
    fprintf(g_log, "[NETDISC] disconnect showmsg=%u from %08X this=%08X\n",
            showMsg, ret, ecx);
    fflush(g_log);
}

void probeNetState(uint32_t ret, uint32_t state) {
    if (!g_log) return;
    fprintf(g_log, "[NETSTATE] reconnect state %u from %08X\n", state, ret);
    fflush(g_log);
}

void probeDrvBind() { if (g_log) { fprintf(g_log, "[DRV ] bind step\n"); fflush(g_log); } }


// slot attach first arg is the bone slot name second is the model
void* g_realAttach = nullptr;
Trampoline g_attachTr;
constexpr uintptr_t kAttach = 0x00443D10;

void probeAttach(const char* slot, const char* model) {
    if (!g_log) return;
    fprintf(g_log, "[SLOT] bone=%s model=%s\n",
            okStr(slot), okStr(model));
    fflush(g_log);
}

// quest and room craft are dead sub 42CF50 skips them sub 42BCE0 handles ids added via sub 44C580
void* g_realAdd = nullptr;
Trampoline g_addTr;
constexpr uintptr_t kAddButton = 0x0044C580;

using addbtn_t = char(__thiscall*)(void*, const char*, int, int, int, int, int);

bool g_addingBar = false;

// spacing on the real bar places quest between ghost and tutorial room craft after carcraft
void afterAddButton(void* group, const char* name) {
    if (g_addingBar || !name) return;
    if (strcmp(name, "Menu/Common_Bottom_Gacha_") != 0) return;

    g_addingBar = true;
    auto add = reinterpret_cast<addbtn_t>(g_realAdd);
    // pak wins over the loose tree sub 44C580 falls back to shipped names if ours are missing
    char okQuest = add(group, "Menu/KncQuest_", 449, 4, 15, 0, 0);
    if (!okQuest) okQuest = add(group, "Menu/Common_Top_Quest_", 449, 4, 15, 0, 0);
    char okRoom  = add(group, "Menu/KncRoomCraft_", 606, 731, 9, 0, 0);
    if (!okRoom)  okRoom  = add(group, "Menu/Common_Bottom_RoomCraft_", 606, 731, 9, 0, 0);
    g_addingBar = false;

    if (g_log) {
        fprintf(g_log, "[BAR] quest=%d roomcraft=%d group=%08X\n",
                okQuest, okRoom, static_cast<unsigned>(reinterpret_cast<uintptr_t>(group)));
        fflush(g_log);
    }
}

// camera fields zeroed by sub 48DB30 never restored seeded once in sub 43ED70 not sub 43F040
void* g_realCamSet = nullptr;
Trampoline g_camTr;
constexpr uintptr_t kCamSet     = 0x0043ED70;
constexpr uintptr_t kCarCamBase = 0x01B1C510;
constexpr uintptr_t kCarStride  = 0xA7260;
constexpr int       kCarSlots   = 16;

// measured live against the pose the client itself forces on stage 13
constexpr uint32_t kCamDist   = 0x41066666u;
constexpr uint32_t kCamMiddle = 0x40400000u;
constexpr uint32_t kCamHeight = 0x40600000u;  // 8 point 4 units back 3 units middle height 3 point 5 units up

bool g_camFix = true;
bool g_injectButtons = true;

void fillCarCam() {
    if (!g_camFix) return;
    for (int i = 0; i < kCarSlots; ++i) {
        uint32_t* p = reinterpret_cast<uint32_t*>(kCarCamBase + kCarStride * uintptr_t(i));
        if (p[0] != 0u || p[1] != 0u || p[2] != 0u) continue;
        p[0] = kCamDist;
        p[1] = kCamMiddle;
        p[2] = kCamHeight;
    }
}

// thiscall three stack args seeds then falls through to the original
__declspec(naked) void hookCamSet() {
    __asm {
        pushad
        pushfd
        call fillCarCam
        popfd
        popad
        jmp  dword ptr [g_realCamSet]
    }
}

// thiscall six stack args original cleans them with ret 0x18
__declspec(naked) void hookAddButton() {
    __asm {
        push ebp
        mov  ebp, esp
        push esi
        mov  esi, ecx
        push dword ptr [ebp + 28]
        push dword ptr [ebp + 24]
        push dword ptr [ebp + 20]
        push dword ptr [ebp + 16]
        push dword ptr [ebp + 12]
        push dword ptr [ebp + 8]
        mov  ecx, esi
        call dword ptr [g_realAdd]
        push eax
        test al, al
        je   addDone
        pushad
        pushfd
        push dword ptr [ebp + 8]
        push esi
        call afterAddButton
        add  esp, 8
        popfd
        popad
    addDone:
        pop  eax
        pop  esi
        mov  esp, ebp
        pop  ebp
        ret  24
    }
}

__declspec(naked) void hookAttach() {
    __asm {
        pushad
        pushfd
        mov  eax, [esp + 0x2C]
        push eax
        mov  eax, [esp + 0x2C]
        push eax
        call probeAttach
        add  esp, 8
        popfd
        popad
        jmp  dword ptr [g_realAttach]
    }
}

__declspec(naked) void hookLoadModel() {
    __asm {
        pushad
        pushfd
        mov  eax, [esp + 0x2C]
        push eax
        mov  eax, [esp + 0x2C]
        push eax
        call probeLoad
        add  esp, 8
        popfd
        popad
        jmp  dword ptr [g_realLoadModel]
    }
}
__declspec(naked) void hookPartLk() {
    __asm {
        pushad
        pushfd
        mov  eax, [esp + 0x28]
        push eax
        push ecx
        call probePartLk
        add  esp, 8
        popfd
        popad
        jmp  dword ptr [g_realPartLk]
    }
}
__declspec(naked) void hookItemLk() {
    __asm {
        pushad
        pushfd
        mov  eax, [esp + 0x28]
        push eax
        push ecx
        call probeItemLk
        add  esp, 8
        popfd
        popad
        jmp  dword ptr [g_realItemLk]
    }
}
__declspec(naked) void hookMsgBox() {
    __asm {
        pushad
        pushfd
        mov  eax, [esp + 0x28]
        push eax
        push ecx
        mov  eax, [esp + 0x2C]
        push eax
        call probeMsgBox
        add  esp, 0x0C
        popfd
        popad
        jmp  dword ptr [g_realMsgBox]
    }
}
__declspec(naked) void hookShowMsg() {
    __asm {
        pushad
        pushfd
        mov  eax, [esp + 0x2C]
        push eax
        mov  eax, [esp + 0x2C]
        push eax
        push ecx
        mov  eax, [esp + 0x30]
        push eax
        call probeShowMsg
        add  esp, 0x10
        popfd
        popad
        jmp  dword ptr [g_realShowMsg]
    }
}
__declspec(naked) void hookNetConn() {
    __asm {
        pushad
        pushfd
        mov  eax, [esp + 0x2C]
        push eax
        mov  eax, [esp + 0x2C]
        push eax
        mov  eax, [esp + 0x2C]
        push eax
        call probeNetConn
        add  esp, 0x0C
        popfd
        popad
        jmp  dword ptr [g_realNetConn]
    }
}
__declspec(naked) void hookNetDisc() {
    __asm {
        pushad
        pushfd
        mov  eax, [esp + 0x28]
        push eax
        push ecx
        mov  eax, [esp + 0x2C]
        push eax
        call probeNetDisc
        add  esp, 0x0C
        popfd
        popad
        jmp  dword ptr [g_realNetDisc]
    }
}
__declspec(naked) void hookNetState() {
    __asm {
        pushad
        pushfd
        mov  eax, [esp + 0x28]
        push eax
        mov  eax, [esp + 0x28]
        push eax
        call probeNetState
        add  esp, 8
        popfd
        popad
        jmp  dword ptr [g_realNetState]
    }
}
__declspec(naked) void hookDrvLoad() {
    __asm {
        pushad
        pushfd
        mov  eax, [esp + 0x2C]
        push eax
        mov  eax, [esp + 0x2C]
        push eax
        call probeDrvLoad
        add  esp, 8
        popfd
        popad
        jmp  dword ptr [g_realDrvLoad]
    }
}
__declspec(naked) void hookDrvBind() {
    __asm { pushad
            pushfd
            call probeDrvBind
            popfd
            popad
            jmp dword ptr [g_realDrvBind] }
}
__declspec(naked) void hookFitModel() {
    __asm { pushad
            pushfd
            call probeFit
            popfd
            popad
            jmp dword ptr [g_realFitModel] }
}

__declspec(naked) void hookBuildModel() {
    __asm { pushad
            pushfd
            mov  eax, [esp + 0x2C]
            push eax
            mov  eax, [esp + 0x2C]
            push eax
            call probeBuildArgs
            add  esp, 8
            popfd
            popad
            jmp dword ptr [g_realBuildModel] }
}

// does the 0xC1 part record actually reach the container the model builder reads
void* g_realPartAdd = nullptr;
Trampoline g_partAddTr;
constexpr uintptr_t kPartAdd = 0x00450F40;

void onPartAdd(uintptr_t container, const uint32_t* rec) {
    if (!g_log) return;
    const int count = *reinterpret_cast<const int*>(container + 0x1B804);
    fprintf(g_log, "[PART] container=%08X count=%d key=%u name=%s\n",
            static_cast<unsigned>(container), count,
            rec ? rec[2] : 0,
            rec ? okStr(reinterpret_cast<const char*>(rec) + 16) : "?");
    fflush(g_log);
}

void* g_realC1 = nullptr;
Trampoline g_c1Tr;
constexpr uintptr_t kC1Handler = 0x0047F800;
void probeC1() { if (g_log) { fprintf(g_log, "[C1] handler entered\n"); fflush(g_log); } }
__declspec(naked) void hookC1() {
    __asm { pushad
            pushfd
            call probeC1
            popfd
            popad
            jmp dword ptr [g_realC1] }
}

// ground truth for routing read straight off the reader cursor
void* g_realDispatch = nullptr;
Trampoline g_dispTr;
constexpr uintptr_t kDispatch = 0x004777C0;

void onDispatch(const uint8_t** reader) {
    if (!g_log || !reader) return;
    const uint8_t* cur = *reinterpret_cast<const uint8_t* const*>(
        reinterpret_cast<const uint8_t*>(reader) + 8);
    if (!cur) return;
    const uint16_t len = static_cast<uint16_t>(cur[0] | (cur[1] << 8));
    const uint16_t op  = static_cast<uint16_t>(cur[2] | (cur[3] << 8));
    fprintf(g_log, "[DISP] op=0x%04X len=%u\n", op, len);
    fflush(g_log);
}

__declspec(naked) void hookDispatch() {
    __asm {
        pushad
        pushfd
        mov  eax, [esp + 0x28]
        push eax
        call onDispatch
        add  esp, 4
        popfd
        popad
        jmp  dword ptr [g_realDispatch]
    }
}

__declspec(naked) void hookPartAdd() {
    __asm {
        pushad
        pushfd
        mov  eax, [esp + 0x28]
        push eax
        push ecx
        call onPartAdd
        add  esp, 8
        popfd
        popad
        jmp  dword ptr [g_realPartAdd]
    }
}

__declspec(naked) void hookSetBody() {
    __asm {
        pushad
        pushfd
        mov  eax, [esp + 0x34]
        push eax
        mov  eax, [esp + 0x34]
        push eax
        mov  eax, [esp + 0x34]
        push eax
        mov  eax, [esp + 0x34]
        push eax
        push ecx
        call onSetBody
        add  esp, 20
        popfd
        popad
        jmp  dword ptr [g_realSetBody]
    }
}

__declspec(naked) void hookArm() {
    __asm {
        pushad
        pushfd
        push ecx
        call onArm
        add  esp, 4
        mov  g_skipArm, eax
        popfd
        popad
        cmp  dword ptr [g_skipArm], 0
        jne  armSkip
        jmp  dword ptr [g_realArm]
    armSkip:
        mov  al, 1
        ret
    }
}

// thiscall so this is in ecx and the byte count sits above the return address
__declspec(naked) void hookParse() {
    __asm {
        pushad
        pushfd
        mov  eax, [esp + 0x28]
        push eax
        push ecx
        call onParse
        add  esp, 8
        popfd
        popad
        jmp  dword ptr [g_realParse]
    }
}

// allocate a stub near enough for a rel32 jmp back
void* allocTrampoline(void* target, const uint8_t* prologue, size_t prologueLen) {
    void* stub = VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!stub) return nullptr;
    uint8_t* p = static_cast<uint8_t*>(stub);
    memcpy(p, prologue, prologueLen);
    p[prologueLen] = 0xE9;  // jmp rel32 back to target plus prologue
    const intptr_t rel = reinterpret_cast<intptr_t>(target) + prologueLen
                       - (reinterpret_cast<intptr_t>(p) + prologueLen + 5);
    memcpy(p + prologueLen + 1, &rel, 4);
    return stub;
}

// only the windows hotpatch shape five bytes is accepted anything else is refused
size_t prologueLength(const uint8_t* p) {
    const bool hotpatch = p[0] == 0x8B && p[1] == 0xFF &&
                          p[2] == 0x55 && p[3] == 0x8B && p[4] == 0xEC;
    if (hotpatch) return 5;
    // push ebp mov ebp esp and esp minus eight lands on a clean boundary
    if (p[0] == 0x55 && p[1] == 0x8B && p[2] == 0xEC &&
        p[3] == 0x83 && p[4] == 0xE4 && p[5] == 0xF8) return 6;
    // push ecx push esi mov esi ecx first clean boundary is eight bytes
    if (p[0] == 0x51 && p[1] == 0x56 && p[2] == 0x8B && p[3] == 0xF1 &&
        p[4] == 0x83 && p[5] == 0x7E && p[6] == 0x08 && p[7] == 0xFF) return 8;
    // push minus one  push seh handler  seven bytes and nothing is rip relative
    if (p[0] == 0x6A && p[1] == 0xFF && p[2] == 0x68) return 7;
    // sub esp imm8  push esi  push edi  mov esi ecx
    if (p[0] == 0x83 && p[1] == 0xEC && p[3] == 0x56 && p[4] == 0x57 &&
        p[5] == 0x8B && p[6] == 0xF1) return 7;
    // sub esp imm32
    if (p[0] == 0x81 && p[1] == 0xEC) return 6;
    // mov eax ecx mov ecx dword ptr eax plus imm32
    if (p[0] == 0x8B && p[1] == 0xC1 && p[2] == 0x8B && p[3] == 0x88) return 8;
    // push esi push edi mov edi dword ptr esp plus 0x0C
    if (p[0] == 0x56 && p[1] == 0x57 && p[2] == 0x8B && p[3] == 0x7C &&
        p[4] == 0x24 && p[5] == 0x0C) return 6;
    // push esi mov esi dword ptr ecx plus imm32 the container lookup shape
    if (p[0] == 0x56 && p[1] == 0x8B && p[2] == 0xB1) return 7;
    // mov eax dword ptr esp plus 4 mov ecx dword ptr ecx plus 8 the error box
    if (p[0] == 0x8B && p[1] == 0x44 && p[2] == 0x24 && p[3] == 0x04 &&
        p[4] == 0x8B && p[5] == 0x49) return 7;
    // mov al byte ptr ecx plus imm32 the scene render entry sub 43DC20
    if (p[0] == 0x8A && p[1] == 0x81) return 6;
    // push ebx mov ebx ecx mov al byte ptr ebx plus imm32 the message shower
    if (p[0] == 0x53 && p[1] == 0x8B && p[2] == 0xD9 &&
        p[3] == 0x8A && p[4] == 0x83) return 9;
    // push esi mov esi ecx mov eax dword ptr esi plus imm32 the disconnect
    if (p[0] == 0x56 && p[1] == 0x8B && p[2] == 0xF1 &&
        p[3] == 0x8B && p[4] == 0x86) return 9;
    // mov eax dword ptr esp plus 4 then a one byte push connect and net state
    if (p[0] == 0x8B && p[1] == 0x44 && p[2] == 0x24 && p[3] == 0x04 &&
        (p[4] == 0x55 || p[4] == 0x56 || p[4] == 0x53 || p[4] == 0x57)) return 5;
    return 0;
}

bool installOne(const char* name, void* target, void* detour,
                void** realOut, Trampoline& tr) {
    uint8_t* p = static_cast<uint8_t*>(target);
    const size_t plen = prologueLength(p);
    if (plen < 5) {
        fprintf(g_log, "spy refuse %s unexpected prologue %02X %02X %02X %02X %02X\n",
                name, p[0], p[1], p[2], p[3], p[4]);
        return false;
    }

    void* stub = allocTrampoline(target, p, plen);
    if (!stub) {
        fprintf(g_log, "spy refuse %s trampoline alloc failed\n", name);
        return false;
    }

    DWORD old = 0;
    if (!VirtualProtect(target, 5, PAGE_EXECUTE_READWRITE, &old)) {
        fprintf(g_log, "spy refuse %s VirtualProtect failed %lu\n", name, GetLastError());
        return false;
    }
    memcpy(tr.saved, p, 5);
    tr.target = target;

    p[0] = 0xE9;
    const intptr_t rel = reinterpret_cast<intptr_t>(detour)
                       - (reinterpret_cast<intptr_t>(target) + 5);
    memcpy(p + 1, &rel, 4);

    VirtualProtect(target, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), target, 5);

    *realOut = stub;
    tr.armed = true;
    fprintf(g_log, "spy hooked %s at %p prologue %zu bytes\n", name, target, plen);
    return true;
}

void removeOne(Trampoline& tr) {
    if (!tr.armed) return;
    DWORD old = 0;
    if (VirtualProtect(tr.target, 5, PAGE_EXECUTE_READWRITE, &old)) {
        memcpy(tr.target, tr.saved, 5);
        VirtualProtect(tr.target, 5, old, &old);
        FlushInstructionCache(GetCurrentProcess(), tr.target, 5);
    }
    tr.armed = false;
}

}  // namespace

void setDiagLog(FILE* f) { g_diag = f; }

void configure(const Config& cfg) { g_cfg = cfg; g_camFix = cfg.camFix; g_injectButtons = cfg.injectButtons; }

bool install(const char* logPath) {
    if (!g_cfg.enabled) return true;

    // NUL rather than a null FILE pointer so every fprintf stays valid
    g_log = _fsopen(g_cfg.quiet ? "NUL" : logPath, "w", _SH_DENYNO);
    if (!g_log) return false;

    // ws2 32 is a KnownDLL so this is always the real one
    HMODULE ws2 = LoadLibraryA("ws2_32.dll");
    if (!ws2) {
        fprintf(g_log, "spy fatal ws2_32 not loadable\n");
        return false;
    }

    void* pSend = reinterpret_cast<void*>(GetProcAddress(ws2, "send"));
    void* pRecv = reinterpret_cast<void*>(GetProcAddress(ws2, "recv"));
    if (!pSend || !pRecv) {
        fprintf(g_log, "spy fatal send or recv not found\n");
        return false;
    }

    bool ok = installOne("send", pSend, reinterpret_cast<void*>(&hookSend),
                         reinterpret_cast<void**>(&g_realSend), g_sendTr);
    ok = installOne("recv", pRecv, reinterpret_cast<void*>(&hookRecv),
                    reinterpret_cast<void**>(&g_realRecv), g_recvTr) && ok;

    // hooks connect always redirect only fires when redirectIp is set
    if (void* pC = reinterpret_cast<void*>(GetProcAddress(ws2, "connect"))) {
        installOne("connect", pC, reinterpret_cast<void*>(&hookConnect),
                   reinterpret_cast<void**>(&g_realConnect), g_connectTr);
    }

    void* pWS = reinterpret_cast<void*>(GetProcAddress(ws2, "WSASend"));
    void* pWR = reinterpret_cast<void*>(GetProcAddress(ws2, "WSARecv"));
    if (pWS) installOne("WSASend", pWS, reinterpret_cast<void*>(&hookWsaSend),
                        reinterpret_cast<void**>(&g_realWsaSend), g_wsaSendTr);
    if (pWR) installOne("WSARecv", pWR, reinterpret_cast<void*>(&hookWsaRecv),
                        reinterpret_cast<void**>(&g_realWsaRecv), g_wsaRecvTr);

    // asset misses use kernel32 exports carrying the hotpatch prologue our own folder is already the cwd
    GetCurrentDirectoryA(MAX_PATH, g_ourDir);

    // kernel32 only holds jmp thunks the real code lives in kernelbase
    HMODULE fileMod = GetModuleHandleA("kernelbase.dll");
    if (!fileMod) fileMod = GetModuleHandleA("kernel32.dll");
    if (HMODULE k32 = fileMod) {
        if (void* pA = reinterpret_cast<void*>(GetProcAddress(k32, "CreateFileA")))
            installOne("CreateFileA", pA, reinterpret_cast<void*>(&hookCreateFileA),
                       reinterpret_cast<void**>(&g_realCreateFileA), g_cfaTr);
        if (void* pW = reinterpret_cast<void*>(GetProcAddress(k32, "CreateFileW")))
            installOne("CreateFileW", pW, reinterpret_cast<void*>(&hookCreateFileW),
                       reinterpret_cast<void**>(&g_realCreateFileW), g_cfwTr);
        if (g_cfg.cwdFix) {
            if (void* pA = reinterpret_cast<void*>(GetProcAddress(k32, "SetCurrentDirectoryA")))
                installOne("SetCurrentDirectoryA", pA, reinterpret_cast<void*>(&hookSetCwdA),
                           reinterpret_cast<void**>(&g_realSetCwdA), g_scdaTr);
            if (void* pW = reinterpret_cast<void*>(GetProcAddress(k32, "SetCurrentDirectoryW")))
                installOne("SetCurrentDirectoryW", pW, reinterpret_cast<void*>(&hookSetCwdW),
                           reinterpret_cast<void**>(&g_realSetCwdW), g_scdwTr);
        }
    }

    installOne("gradeButton", reinterpret_cast<void*>(kGradeBtn),
               reinterpret_cast<void*>(&hookGradeBtn), &g_realGradeBtn, g_gradeTr);
    installOne("viewport", reinterpret_cast<void*>(kViewport),
               reinterpret_cast<void*>(&hookViewport), &g_realViewport, g_viewTr);
    installOne("buildModel", reinterpret_cast<void*>(kBuildModel),
               reinterpret_cast<void*>(&hookBuildModel), &g_realBuildModel, g_buildTr);
    installOne("sceneRender", reinterpret_cast<void*>(kSceneRender),
               reinterpret_cast<void*>(&hookSceneRender), &g_realSceneRender, g_sceneTr);

    // spy buttons 0 isolates anything the injection might disturb
    if (g_injectButtons) {
        installOne("addButton", reinterpret_cast<void*>(kAddButton),
                   reinterpret_cast<void*>(&hookAddButton), &g_realAdd, g_addTr);
    }

    if (g_camFix) {
        installOne("camSet", reinterpret_cast<void*>(kCamSet),
                   reinterpret_cast<void*>(&hookCamSet), &g_realCamSet, g_camTr);
    }


    installOne("attachSlot", reinterpret_cast<void*>(kAttach),
               reinterpret_cast<void*>(&hookAttach), &g_realAttach, g_attachTr);

    installOne("loadModel", reinterpret_cast<void*>(kLoadModel),
               reinterpret_cast<void*>(&hookLoadModel), &g_realLoadModel, g_loadTr);
    installOne("fitModel", reinterpret_cast<void*>(kFitModel),
               reinterpret_cast<void*>(&hookFitModel), &g_realFitModel, g_fitTr);

    installOne("msgBox", reinterpret_cast<void*>(kMsgBox),
               reinterpret_cast<void*>(&hookMsgBox), &g_realMsgBox, g_msgTr);

    installOne("showMsg", reinterpret_cast<void*>(kShowMsg),
               reinterpret_cast<void*>(&hookShowMsg), &g_realShowMsg, g_showMsgTr);
    installOne("netConnect", reinterpret_cast<void*>(kNetConn),
               reinterpret_cast<void*>(&hookNetConn), &g_realNetConn, g_netConnTr);
    installOne("netDisconnect", reinterpret_cast<void*>(kNetDisc),
               reinterpret_cast<void*>(&hookNetDisc), &g_realNetDisc, g_netDiscTr);
    installOne("netReconnectState", reinterpret_cast<void*>(kNetState),
               reinterpret_cast<void*>(&hookNetState), &g_realNetState, g_netStateTr);

    installOne("partLookup", reinterpret_cast<void*>(kPartLookup),
               reinterpret_cast<void*>(&hookPartLk), &g_realPartLk, g_partLkTr);
    installOne("itemLookup", reinterpret_cast<void*>(kItemLookup),
               reinterpret_cast<void*>(&hookItemLk), &g_realItemLk, g_itemLkTr);

    installOne("driverModel", reinterpret_cast<void*>(kDriverModel),
               reinterpret_cast<void*>(&hookDrvLoad), &g_realDrvLoad, g_drvLoadTr);
    installOne("driverBind", reinterpret_cast<void*>(kDriverBind),
               reinterpret_cast<void*>(&hookDrvBind), &g_realDrvBind, g_drvBindTr);

    installOne("PacketDispatcher", reinterpret_cast<void*>(kDispatch),
               reinterpret_cast<void*>(&hookDispatch), &g_realDispatch, g_dispTr);

    installOne("C1handler", reinterpret_cast<void*>(kC1Handler),
               reinterpret_cast<void*>(&hookC1), &g_realC1, g_c1Tr);

    installOne("partAdd", reinterpret_cast<void*>(kPartAdd),
               reinterpret_cast<void*>(&hookPartAdd), &g_realPartAdd, g_partAddTr);

    installOne("SetBody", reinterpret_cast<void*>(kSetBody),
               reinterpret_cast<void*>(&hookSetBody), &g_realSetBody, g_setBodyTr);

    // clamp first  it must be armed before any read is issued
    if (g_diag) { fprintf(g_diag, "[HOOK] arm guard installing at 0x%X\n", (unsigned)kArmRecv); fflush(g_diag); }
    installOne("NetworkHandler_ArmRecv", reinterpret_cast<void*>(kArmRecv),
               reinterpret_cast<void*>(&hookArm), &g_realArm, g_armTr);

    // the real wire tap  fixed image base so the va is stable
    installOne("ParsePacketsLoop", reinterpret_cast<void*>(kParseLoop),
               reinterpret_cast<void*>(&hookParse), &g_realParse, g_parseTr);

    fprintf(g_log, "spy install %s\n", ok ? "ok" : "partial");
    fflush(g_log);
    return ok;
}

void shutdown() {
    removeOne(g_sendTr);
    removeOne(g_recvTr);
    removeOne(g_parseTr);
    removeOne(g_armTr);
    if (g_log) { fclose(g_log); g_log = nullptr; }
}

}  // namespace spy
