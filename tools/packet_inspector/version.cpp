// version.dll proxy - hooks network by patching WS2_32 at runtime
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdio>
#include <cstdint>
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

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

PFN_send Real_send = nullptr;
PFN_recv Real_recv = nullptr;
PFN_connect Real_connect = nullptr;

// trampoline storage
uint8_t* tramp_send = nullptr;
uint8_t* tramp_recv = nullptr;
uint8_t* tramp_connect = nullptr;

int WSAAPI Hook_send(SOCKET s, const char* buf, int len, int flags) {
    LogPacket(true, buf, len);
    return Real_send(s, buf, len, flags);
}

int WSAAPI Hook_recv(SOCKET s, char* buf, int len, int flags) {
    int result = Real_recv(s, buf, len, flags);
    if (result > 0) LogPacket(false, buf, result);
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

bool InstallHook(void* target, void* hook, uint8_t** trampoline, void** original) {
    // allocate trampoline
    *trampoline = (uint8_t*)VirtualAlloc(NULL, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!*trampoline) return false;
    
    DWORD oldProt;
    VirtualProtect(target, 16, PAGE_EXECUTE_READWRITE, &oldProt);
    
    // copy original bytes to trampoline
    memcpy(*trampoline, target, 5);
    
    // add jump back to original+5
    (*trampoline)[5] = 0xE9;
    *(int32_t*)(*trampoline + 6) = (int32_t)((uint8_t*)target + 5 - (*trampoline + 10));
    
    *original = *trampoline;
    
    // patch target with jump to hook
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
    
    Log(CLR_CYAN, "WS2_32.dll @ %p", ws2);
    
    if (pSend && InstallHook(pSend, Hook_send, &tramp_send, (void**)&Real_send)) {
        Log(CLR_GREEN, "  send() hooked @ %p", pSend);
    }
    if (pRecv && InstallHook(pRecv, Hook_recv, &tramp_recv, (void**)&Real_recv)) {
        Log(CLR_GREEN, "  recv() hooked @ %p", pRecv);
    }
    if (pConnect && InstallHook(pConnect, Hook_connect, &tramp_connect, (void**)&Real_connect)) {
        Log(CLR_GREEN, "  connect() hooked @ %p", pConnect);
    }
    
    Log(CLR_WHITE, "Network hooks installed!");
}

// real version.dll
HINSTANCE hVersion = nullptr;
FARPROC pGetFileVersionInfoA = nullptr;
FARPROC pGetFileVersionInfoByHandle = nullptr;
FARPROC pGetFileVersionInfoExA = nullptr;
FARPROC pGetFileVersionInfoExW = nullptr;
FARPROC pGetFileVersionInfoSizeA = nullptr;
FARPROC pGetFileVersionInfoSizeExA = nullptr;
FARPROC pGetFileVersionInfoSizeExW = nullptr;
FARPROC pGetFileVersionInfoSizeW = nullptr;
FARPROC pGetFileVersionInfoW = nullptr;
FARPROC pVerFindFileA = nullptr;
FARPROC pVerFindFileW = nullptr;
FARPROC pVerInstallFileA = nullptr;
FARPROC pVerInstallFileW = nullptr;
FARPROC pVerLanguageNameA = nullptr;
FARPROC pVerLanguageNameW = nullptr;
FARPROC pVerQueryValueA = nullptr;
FARPROC pVerQueryValueW = nullptr;

bool LoadRealVersion() {
    char path[MAX_PATH];
    GetSystemDirectoryA(path, MAX_PATH);
    strcat_s(path, "\\version.dll");
    
    hVersion = LoadLibraryA(path);
    if (!hVersion) return false;
    
    pGetFileVersionInfoA = GetProcAddress(hVersion, "GetFileVersionInfoA");
    pGetFileVersionInfoByHandle = GetProcAddress(hVersion, "GetFileVersionInfoByHandle");
    pGetFileVersionInfoExA = GetProcAddress(hVersion, "GetFileVersionInfoExA");
    pGetFileVersionInfoExW = GetProcAddress(hVersion, "GetFileVersionInfoExW");
    pGetFileVersionInfoSizeA = GetProcAddress(hVersion, "GetFileVersionInfoSizeA");
    pGetFileVersionInfoSizeExA = GetProcAddress(hVersion, "GetFileVersionInfoSizeExA");
    pGetFileVersionInfoSizeExW = GetProcAddress(hVersion, "GetFileVersionInfoSizeExW");
    pGetFileVersionInfoSizeW = GetProcAddress(hVersion, "GetFileVersionInfoSizeW");
    pGetFileVersionInfoW = GetProcAddress(hVersion, "GetFileVersionInfoW");
    pVerFindFileA = GetProcAddress(hVersion, "VerFindFileA");
    pVerFindFileW = GetProcAddress(hVersion, "VerFindFileW");
    pVerInstallFileA = GetProcAddress(hVersion, "VerInstallFileA");
    pVerInstallFileW = GetProcAddress(hVersion, "VerInstallFileW");
    pVerLanguageNameA = GetProcAddress(hVersion, "VerLanguageNameA");
    pVerLanguageNameW = GetProcAddress(hVersion, "VerLanguageNameW");
    pVerQueryValueA = GetProcAddress(hVersion, "VerQueryValueA");
    pVerQueryValueW = GetProcAddress(hVersion, "VerQueryValueW");
    
    return true;
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
        Log(CLR_CYAN, "       KnC Network Proxy (version.dll)");
        Log(CLR_CYAN, "===============================================");
        
        if (!LoadRealVersion()) {
            Log(CLR_RED, "Failed to load real version.dll!");
            return FALSE;
        }
        Log(CLR_GREEN, "Real version.dll loaded");
        
        InstallNetworkHooks();
        Log(CLR_CYAN, "-----------------------------------------------");
    }
    else if (reason == DLL_PROCESS_DETACH) {
        if (hVersion) FreeLibrary(hVersion);
    }
    return TRUE;
}

// forwarded exports
extern "C" __declspec(naked) void __stdcall GetFileVersionInfoA_() { __asm jmp pGetFileVersionInfoA }
extern "C" __declspec(naked) void __stdcall GetFileVersionInfoByHandle_() { __asm jmp pGetFileVersionInfoByHandle }
extern "C" __declspec(naked) void __stdcall GetFileVersionInfoExA_() { __asm jmp pGetFileVersionInfoExA }
extern "C" __declspec(naked) void __stdcall GetFileVersionInfoExW_() { __asm jmp pGetFileVersionInfoExW }
extern "C" __declspec(naked) void __stdcall GetFileVersionInfoSizeA_() { __asm jmp pGetFileVersionInfoSizeA }
extern "C" __declspec(naked) void __stdcall GetFileVersionInfoSizeExA_() { __asm jmp pGetFileVersionInfoSizeExA }
extern "C" __declspec(naked) void __stdcall GetFileVersionInfoSizeExW_() { __asm jmp pGetFileVersionInfoSizeExW }
extern "C" __declspec(naked) void __stdcall GetFileVersionInfoSizeW_() { __asm jmp pGetFileVersionInfoSizeW }
extern "C" __declspec(naked) void __stdcall GetFileVersionInfoW_() { __asm jmp pGetFileVersionInfoW }
extern "C" __declspec(naked) void __stdcall VerFindFileA_() { __asm jmp pVerFindFileA }
extern "C" __declspec(naked) void __stdcall VerFindFileW_() { __asm jmp pVerFindFileW }
extern "C" __declspec(naked) void __stdcall VerInstallFileA_() { __asm jmp pVerInstallFileA }
extern "C" __declspec(naked) void __stdcall VerInstallFileW_() { __asm jmp pVerInstallFileW }
extern "C" __declspec(naked) void __stdcall VerLanguageNameA_() { __asm jmp pVerLanguageNameA }
extern "C" __declspec(naked) void __stdcall VerLanguageNameW_() { __asm jmp pVerLanguageNameW }
extern "C" __declspec(naked) void __stdcall VerQueryValueA_() { __asm jmp pVerQueryValueA }
extern "C" __declspec(naked) void __stdcall VerQueryValueW_() { __asm jmp pVerQueryValueW }

