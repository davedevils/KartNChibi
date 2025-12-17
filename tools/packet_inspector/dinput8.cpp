// dinput8.dll proxy - hooks network by patching WS2_32 at runtime
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <winsock2.h>
#include <cstdio>
#include <cstdint>

// console colors
#define CLR_RESET   "\033[0m"
#define CLR_RED     "\033[91m"
#define CLR_GREEN   "\033[92m"
#define CLR_YELLOW  "\033[93m"
#define CLR_CYAN    "\033[96m"
#define CLR_WHITE   "\033[97m"

void Log(const char* color, const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    
    SYSTEMTIME st;
    GetLocalTime(&st);
    printf("%s[%02d:%02d:%02d.%03d] %s%s\n", 
        color, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
        buf, CLR_RESET);
}

const char* GetPacketName(uint8_t cmd) {
    switch(cmd) {
        case 0x02: return "DISPLAY_MESSAGE";
        case 0x03: return "CHAR_CREATE_PROMPT";
        case 0x04: return "CHAR_CREATE_RESULT";
        case 0x07: return "SESSION_INFO";
        case 0x0A: return "CONNECTION_OK";
        case 0x0E: return "CHANNEL_LIST";
        case 0x12: return "SHOW_LOBBY";
        case 0x18: return "CHANNEL_SELECT";
        case 0x3F: return "ROOM_LIST";
        case 0xA6: return "HEARTBEAT";
        case 0xA7: return "SESSION_CONFIRM";
        case 0xFE: return "LAUNCHER_AUTH";
        default: return "UNKNOWN";
    }
}

void LogPacket(bool isSend, const char* buf, int len) {
    if (len < 7) return;
    
    uint8_t cmd = ((uint8_t*)buf)[6];
    if (cmd == 0xA6) return;  // skip heartbeat
    
    const char* name = GetPacketName(cmd);
    const char* dir = isSend ? ">>>" : "<<<";
    const char* color = isSend ? CLR_GREEN : CLR_RED;
    const char* type = isSend ? "CtoS" : "StoC";
    
    Log(color, "[%s] %s CMD=0x%02X %s (%d bytes)", type, dir, cmd, name, len);
    
    char hex[256] = {0};
    int n = len > 32 ? 32 : len;
    for (int i = 0; i < n; i++) {
        sprintf(hex + i*3, "%02X ", (uint8_t)buf[i]);
    }
    Log(color, "  %s", hex);
}

// original function pointers
typedef int (WSAAPI* PFN_send)(SOCKET, const char*, int, int);
typedef int (WSAAPI* PFN_recv)(SOCKET, char*, int, int);
typedef int (WSAAPI* PFN_connect)(SOCKET, const sockaddr*, int);
typedef int (WSAAPI* PFN_WSASend)(SOCKET, LPWSABUF, DWORD, LPDWORD, DWORD, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);
typedef int (WSAAPI* PFN_WSARecv)(SOCKET, LPWSABUF, DWORD, LPDWORD, LPDWORD, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);

PFN_send Real_send = nullptr;
PFN_recv Real_recv = nullptr;
PFN_connect Real_connect = nullptr;
PFN_WSASend Real_WSASend = nullptr;
PFN_WSARecv Real_WSARecv = nullptr;

int WSAAPI Hook_send(SOCKET s, const char* buf, int len, int flags) {
    LogPacket(true, buf, len);
    return Real_send(s, buf, len, flags);
}

int WSAAPI Hook_recv(SOCKET s, char* buf, int len, int flags) {
    int result = Real_recv(s, buf, len, flags);
    if (result > 0) LogPacket(false, buf, result);
    return result;
}

int WSAAPI Hook_WSASend(SOCKET s, LPWSABUF bufs, DWORD cnt, LPDWORD sent, DWORD fl, LPWSAOVERLAPPED ov, LPWSAOVERLAPPED_COMPLETION_ROUTINE cr) {
    for (DWORD i = 0; i < cnt; i++) {
        if (bufs[i].buf && bufs[i].len > 0)
            LogPacket(true, bufs[i].buf, bufs[i].len);
    }
    return Real_WSASend(s, bufs, cnt, sent, fl, ov, cr);
}

int WSAAPI Hook_WSARecv(SOCKET s, LPWSABUF bufs, DWORD cnt, LPDWORD recvd, LPDWORD fl, LPWSAOVERLAPPED ov, LPWSAOVERLAPPED_COMPLETION_ROUTINE cr) {
    int result = Real_WSARecv(s, bufs, cnt, recvd, fl, ov, cr);
    // for sync recv, log immediately
    if (result == 0 && recvd && *recvd > 0) {
        for (DWORD i = 0; i < cnt; i++) {
            if (bufs[i].buf && bufs[i].len > 0)
                LogPacket(false, bufs[i].buf, *recvd);
        }
    }
    return result;
}

int WSAAPI Hook_connect(SOCKET s, const sockaddr* name, int namelen) {
    if (name->sa_family == AF_INET) {
        sockaddr_in* addr = (sockaddr_in*)name;
        DWORD ip = addr->sin_addr.S_un.S_addr;
        int port = ((addr->sin_port & 0xFF) << 8) | ((addr->sin_port >> 8) & 0xFF);
        Log(CLR_CYAN, "===============================================");
        Log(CLR_CYAN, "  CONNECT TO %d.%d.%d.%d:%d", 
            ip & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, (ip >> 24) & 0xFF, port);
        Log(CLR_CYAN, "===============================================");
    }
    return Real_connect(s, name, namelen);
}

bool InstallHook(void* target, void* hook, void** original) {
    uint8_t* tramp = (uint8_t*)VirtualAlloc(NULL, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tramp) return false;
    
    DWORD oldProt;
    VirtualProtect(target, 16, PAGE_EXECUTE_READWRITE, &oldProt);
    
    memcpy(tramp, target, 5);
    tramp[5] = 0xE9;
    *(int32_t*)(tramp + 6) = (int32_t)((uint8_t*)target + 5 - (tramp + 10));
    
    *original = tramp;
    
    ((uint8_t*)target)[0] = 0xE9;
    *(int32_t*)((uint8_t*)target + 1) = (int32_t)((uint8_t*)hook - ((uint8_t*)target + 5));
    
    VirtualProtect(target, 16, oldProt, &oldProt);
    FlushInstructionCache(GetCurrentProcess(), target, 16);
    
    return true;
}

void InstallNetworkHooks() {
    HMODULE ws2 = GetModuleHandleA("WS2_32.dll");
    if (!ws2) ws2 = LoadLibraryA("WS2_32.dll");
    if (!ws2) {
        Log(CLR_RED, "WS2_32.dll not found!");
        return;
    }
    
    void* pSend = GetProcAddress(ws2, "send");
    void* pRecv = GetProcAddress(ws2, "recv");
    void* pConnect = GetProcAddress(ws2, "connect");
    void* pWSASend = GetProcAddress(ws2, "WSASend");
    void* pWSARecv = GetProcAddress(ws2, "WSARecv");
    
    Log(CLR_CYAN, "WS2_32.dll @ %p", ws2);
    
    int hooked = 0;
    if (pSend && InstallHook(pSend, Hook_send, (void**)&Real_send)) {
        Log(CLR_GREEN, "  send() @ %p", pSend);
        hooked++;
    }
    if (pRecv && InstallHook(pRecv, Hook_recv, (void**)&Real_recv)) {
        Log(CLR_GREEN, "  recv() @ %p", pRecv);
        hooked++;
    }
    if (pConnect && InstallHook(pConnect, Hook_connect, (void**)&Real_connect)) {
        Log(CLR_GREEN, "  connect() @ %p", pConnect);
        hooked++;
    }
    if (pWSASend && InstallHook(pWSASend, Hook_WSASend, (void**)&Real_WSASend)) {
        Log(CLR_GREEN, "  WSASend() @ %p", pWSASend);
        hooked++;
    }
    if (pWSARecv && InstallHook(pWSARecv, Hook_WSARecv, (void**)&Real_WSARecv)) {
        Log(CLR_GREEN, "  WSARecv() @ %p", pWSARecv);
        hooked++;
    }
    
    Log(CLR_WHITE, "Hooked %d functions!", hooked);
}

// real dinput8.dll
HINSTANCE hDinput8 = nullptr;

typedef HRESULT (WINAPI* PFN_DirectInput8Create)(HINSTANCE, DWORD, const IID&, LPVOID*, IUnknown*);
PFN_DirectInput8Create Real_DirectInput8Create = nullptr;

bool LoadRealDinput8() {
    char path[MAX_PATH];
    GetSystemDirectoryA(path, MAX_PATH);
    strcat_s(path, "\\dinput8.dll");
    
    hDinput8 = LoadLibraryA(path);
    if (!hDinput8) return false;
    
    Real_DirectInput8Create = (PFN_DirectInput8Create)GetProcAddress(hDinput8, "DirectInput8Create");
    return Real_DirectInput8Create != nullptr;
}

// exported function
extern "C" __declspec(dllexport) HRESULT WINAPI DirectInput8Create(
    HINSTANCE hinst, DWORD dwVersion, const IID& riidltf, LPVOID* ppvOut, IUnknown* punkOuter) {
    return Real_DirectInput8Create(hinst, dwVersion, riidltf, ppvOut, punkOuter);
}

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hInst);
        
        AllocConsole();
        freopen("CONOUT$", "w", stdout);
        
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode;
        GetConsoleMode(hOut, &mode);
        SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        
        Log(CLR_CYAN, "===============================================");
        Log(CLR_CYAN, "       KnC Network Proxy (dinput8.dll)");
        Log(CLR_CYAN, "===============================================");
        
        if (!LoadRealDinput8()) {
            Log(CLR_RED, "Failed to load real dinput8.dll!");
            return FALSE;
        }
        Log(CLR_GREEN, "Real dinput8.dll loaded");
        
        InstallNetworkHooks();
        Log(CLR_CYAN, "-----------------------------------------------");
    }
    else if (reason == DLL_PROCESS_DETACH) {
        if (hDinput8) FreeLibrary(hDinput8);
    }
    return TRUE;
}

