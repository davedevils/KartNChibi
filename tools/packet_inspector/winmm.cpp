// winmm.dll proxy - hooks network by patching WS2_32 at runtime
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

bool InstallHook(void* target, void* hook, void** original) {
    // allocate trampoline
    uint8_t* tramp = (uint8_t*)VirtualAlloc(NULL, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tramp) return false;
    
    DWORD oldProt;
    VirtualProtect(target, 16, PAGE_EXECUTE_READWRITE, &oldProt);
    
    // copy original bytes to trampoline
    memcpy(tramp, target, 5);
    
    // add jump back to original+5
    tramp[5] = 0xE9;
    *(int32_t*)(tramp + 6) = (int32_t)((uint8_t*)target + 5 - (tramp + 10));
    
    *original = tramp;
    
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
    
    if (pSend && InstallHook(pSend, Hook_send, (void**)&Real_send)) {
        Log(CLR_GREEN, "  send() hooked @ %p", pSend);
    }
    if (pRecv && InstallHook(pRecv, Hook_recv, (void**)&Real_recv)) {
        Log(CLR_GREEN, "  recv() hooked @ %p", pRecv);
    }
    if (pConnect && InstallHook(pConnect, Hook_connect, (void**)&Real_connect)) {
        Log(CLR_GREEN, "  connect() hooked @ %p", pConnect);
    }
    
    Log(CLR_WHITE, "Network hooks installed!");
}

// real winmm.dll
HINSTANCE hWinmm = nullptr;

// winmm function pointers
#define WINMM_FUNC(name) FARPROC p##name = nullptr;
WINMM_FUNC(timeGetTime)
WINMM_FUNC(timeBeginPeriod)
WINMM_FUNC(timeEndPeriod)
WINMM_FUNC(timeGetDevCaps)
WINMM_FUNC(timeGetSystemTime)
WINMM_FUNC(timeSetEvent)
WINMM_FUNC(timeKillEvent)
WINMM_FUNC(mciSendCommandA)
WINMM_FUNC(mciSendCommandW)
WINMM_FUNC(mciSendStringA)
WINMM_FUNC(mciSendStringW)
WINMM_FUNC(mciGetErrorStringA)
WINMM_FUNC(mciGetErrorStringW)
WINMM_FUNC(mciGetDeviceIDA)
WINMM_FUNC(mciGetDeviceIDW)
WINMM_FUNC(mciGetDeviceIDFromElementIDA)
WINMM_FUNC(mciGetDeviceIDFromElementIDW)
WINMM_FUNC(mciExecute)
WINMM_FUNC(mciSetYieldProc)
WINMM_FUNC(mciGetYieldProc)
WINMM_FUNC(mciGetCreatorTask)
WINMM_FUNC(mciDriverNotify)
WINMM_FUNC(mciDriverYield)
WINMM_FUNC(mmioOpenA)
WINMM_FUNC(mmioOpenW)
WINMM_FUNC(mmioClose)
WINMM_FUNC(mmioRead)
WINMM_FUNC(mmioWrite)
WINMM_FUNC(mmioSeek)
WINMM_FUNC(mmioGetInfo)
WINMM_FUNC(mmioSetInfo)
WINMM_FUNC(mmioSetBuffer)
WINMM_FUNC(mmioFlush)
WINMM_FUNC(mmioAdvance)
WINMM_FUNC(mmioStringToFOURCCA)
WINMM_FUNC(mmioStringToFOURCCW)
WINMM_FUNC(mmioInstallIOProcA)
WINMM_FUNC(mmioInstallIOProcW)
WINMM_FUNC(mmioSendMessage)
WINMM_FUNC(mmioDescend)
WINMM_FUNC(mmioAscend)
WINMM_FUNC(mmioCreateChunk)
WINMM_FUNC(mmioRenameA)
WINMM_FUNC(mmioRenameW)
WINMM_FUNC(waveOutGetNumDevs)
WINMM_FUNC(waveOutGetDevCapsA)
WINMM_FUNC(waveOutGetDevCapsW)
WINMM_FUNC(waveOutGetVolume)
WINMM_FUNC(waveOutSetVolume)
WINMM_FUNC(waveOutGetErrorTextA)
WINMM_FUNC(waveOutGetErrorTextW)
WINMM_FUNC(waveOutOpen)
WINMM_FUNC(waveOutClose)
WINMM_FUNC(waveOutPrepareHeader)
WINMM_FUNC(waveOutUnprepareHeader)
WINMM_FUNC(waveOutWrite)
WINMM_FUNC(waveOutPause)
WINMM_FUNC(waveOutRestart)
WINMM_FUNC(waveOutReset)
WINMM_FUNC(waveOutBreakLoop)
WINMM_FUNC(waveOutGetPosition)
WINMM_FUNC(waveOutGetPitch)
WINMM_FUNC(waveOutSetPitch)
WINMM_FUNC(waveOutGetPlaybackRate)
WINMM_FUNC(waveOutSetPlaybackRate)
WINMM_FUNC(waveOutGetID)
WINMM_FUNC(waveOutMessage)
WINMM_FUNC(waveInGetNumDevs)
WINMM_FUNC(waveInGetDevCapsA)
WINMM_FUNC(waveInGetDevCapsW)
WINMM_FUNC(waveInGetErrorTextA)
WINMM_FUNC(waveInGetErrorTextW)
WINMM_FUNC(waveInOpen)
WINMM_FUNC(waveInClose)
WINMM_FUNC(waveInPrepareHeader)
WINMM_FUNC(waveInUnprepareHeader)
WINMM_FUNC(waveInAddBuffer)
WINMM_FUNC(waveInStart)
WINMM_FUNC(waveInStop)
WINMM_FUNC(waveInReset)
WINMM_FUNC(waveInGetPosition)
WINMM_FUNC(waveInGetID)
WINMM_FUNC(waveInMessage)
WINMM_FUNC(midiOutGetNumDevs)
WINMM_FUNC(midiStreamOpen)
WINMM_FUNC(midiStreamClose)
WINMM_FUNC(midiStreamProperty)
WINMM_FUNC(midiStreamPosition)
WINMM_FUNC(midiStreamOut)
WINMM_FUNC(midiStreamPause)
WINMM_FUNC(midiStreamRestart)
WINMM_FUNC(midiStreamStop)
WINMM_FUNC(midiConnect)
WINMM_FUNC(midiDisconnect)
WINMM_FUNC(midiOutGetDevCapsA)
WINMM_FUNC(midiOutGetDevCapsW)
WINMM_FUNC(midiOutGetVolume)
WINMM_FUNC(midiOutSetVolume)
WINMM_FUNC(midiOutGetErrorTextA)
WINMM_FUNC(midiOutGetErrorTextW)
WINMM_FUNC(midiOutOpen)
WINMM_FUNC(midiOutClose)
WINMM_FUNC(midiOutPrepareHeader)
WINMM_FUNC(midiOutUnprepareHeader)
WINMM_FUNC(midiOutShortMsg)
WINMM_FUNC(midiOutLongMsg)
WINMM_FUNC(midiOutReset)
WINMM_FUNC(midiOutCachePatches)
WINMM_FUNC(midiOutCacheDrumPatches)
WINMM_FUNC(midiOutGetID)
WINMM_FUNC(midiOutMessage)
WINMM_FUNC(midiInGetNumDevs)
WINMM_FUNC(midiInGetDevCapsA)
WINMM_FUNC(midiInGetDevCapsW)
WINMM_FUNC(midiInGetErrorTextA)
WINMM_FUNC(midiInGetErrorTextW)
WINMM_FUNC(midiInOpen)
WINMM_FUNC(midiInClose)
WINMM_FUNC(midiInPrepareHeader)
WINMM_FUNC(midiInUnprepareHeader)
WINMM_FUNC(midiInAddBuffer)
WINMM_FUNC(midiInStart)
WINMM_FUNC(midiInStop)
WINMM_FUNC(midiInReset)
WINMM_FUNC(midiInGetID)
WINMM_FUNC(midiInMessage)
WINMM_FUNC(auxGetNumDevs)
WINMM_FUNC(auxGetDevCapsA)
WINMM_FUNC(auxGetDevCapsW)
WINMM_FUNC(auxSetVolume)
WINMM_FUNC(auxGetVolume)
WINMM_FUNC(auxOutMessage)
WINMM_FUNC(mixerGetNumDevs)
WINMM_FUNC(mixerGetDevCapsA)
WINMM_FUNC(mixerGetDevCapsW)
WINMM_FUNC(mixerOpen)
WINMM_FUNC(mixerClose)
WINMM_FUNC(mixerMessage)
WINMM_FUNC(mixerGetLineInfoA)
WINMM_FUNC(mixerGetLineInfoW)
WINMM_FUNC(mixerGetID)
WINMM_FUNC(mixerGetLineControlsA)
WINMM_FUNC(mixerGetLineControlsW)
WINMM_FUNC(mixerGetControlDetailsA)
WINMM_FUNC(mixerGetControlDetailsW)
WINMM_FUNC(mixerSetControlDetails)
WINMM_FUNC(joyGetNumDevs)
WINMM_FUNC(joyGetDevCapsA)
WINMM_FUNC(joyGetDevCapsW)
WINMM_FUNC(joyGetPos)
WINMM_FUNC(joyGetPosEx)
WINMM_FUNC(joyGetThreshold)
WINMM_FUNC(joyReleaseCapture)
WINMM_FUNC(joySetCapture)
WINMM_FUNC(joySetThreshold)
WINMM_FUNC(PlaySoundA)
WINMM_FUNC(PlaySoundW)
WINMM_FUNC(sndPlaySoundA)
WINMM_FUNC(sndPlaySoundW)

bool LoadRealWinmm() {
    char path[MAX_PATH];
    GetSystemDirectoryA(path, MAX_PATH);
    strcat_s(path, "\\winmm.dll");
    
    hWinmm = LoadLibraryA(path);
    if (!hWinmm) return false;
    
    #define LOAD_FUNC(name) p##name = GetProcAddress(hWinmm, #name);
    LOAD_FUNC(timeGetTime)
    LOAD_FUNC(timeBeginPeriod)
    LOAD_FUNC(timeEndPeriod)
    LOAD_FUNC(timeGetDevCaps)
    LOAD_FUNC(timeGetSystemTime)
    LOAD_FUNC(timeSetEvent)
    LOAD_FUNC(timeKillEvent)
    LOAD_FUNC(mciSendCommandA)
    LOAD_FUNC(mciSendCommandW)
    LOAD_FUNC(mciSendStringA)
    LOAD_FUNC(mciSendStringW)
    LOAD_FUNC(mciGetErrorStringA)
    LOAD_FUNC(mciGetErrorStringW)
    LOAD_FUNC(mciGetDeviceIDA)
    LOAD_FUNC(mciGetDeviceIDW)
    LOAD_FUNC(mciGetDeviceIDFromElementIDA)
    LOAD_FUNC(mciGetDeviceIDFromElementIDW)
    LOAD_FUNC(mciExecute)
    LOAD_FUNC(mciSetYieldProc)
    LOAD_FUNC(mciGetYieldProc)
    LOAD_FUNC(mciGetCreatorTask)
    LOAD_FUNC(mciDriverNotify)
    LOAD_FUNC(mciDriverYield)
    LOAD_FUNC(mmioOpenA)
    LOAD_FUNC(mmioOpenW)
    LOAD_FUNC(mmioClose)
    LOAD_FUNC(mmioRead)
    LOAD_FUNC(mmioWrite)
    LOAD_FUNC(mmioSeek)
    LOAD_FUNC(mmioGetInfo)
    LOAD_FUNC(mmioSetInfo)
    LOAD_FUNC(mmioSetBuffer)
    LOAD_FUNC(mmioFlush)
    LOAD_FUNC(mmioAdvance)
    LOAD_FUNC(mmioStringToFOURCCA)
    LOAD_FUNC(mmioStringToFOURCCW)
    LOAD_FUNC(mmioInstallIOProcA)
    LOAD_FUNC(mmioInstallIOProcW)
    LOAD_FUNC(mmioSendMessage)
    LOAD_FUNC(mmioDescend)
    LOAD_FUNC(mmioAscend)
    LOAD_FUNC(mmioCreateChunk)
    LOAD_FUNC(mmioRenameA)
    LOAD_FUNC(mmioRenameW)
    LOAD_FUNC(waveOutGetNumDevs)
    LOAD_FUNC(waveOutGetDevCapsA)
    LOAD_FUNC(waveOutGetDevCapsW)
    LOAD_FUNC(waveOutGetVolume)
    LOAD_FUNC(waveOutSetVolume)
    LOAD_FUNC(waveOutGetErrorTextA)
    LOAD_FUNC(waveOutGetErrorTextW)
    LOAD_FUNC(waveOutOpen)
    LOAD_FUNC(waveOutClose)
    LOAD_FUNC(waveOutPrepareHeader)
    LOAD_FUNC(waveOutUnprepareHeader)
    LOAD_FUNC(waveOutWrite)
    LOAD_FUNC(waveOutPause)
    LOAD_FUNC(waveOutRestart)
    LOAD_FUNC(waveOutReset)
    LOAD_FUNC(waveOutBreakLoop)
    LOAD_FUNC(waveOutGetPosition)
    LOAD_FUNC(waveOutGetPitch)
    LOAD_FUNC(waveOutSetPitch)
    LOAD_FUNC(waveOutGetPlaybackRate)
    LOAD_FUNC(waveOutSetPlaybackRate)
    LOAD_FUNC(waveOutGetID)
    LOAD_FUNC(waveOutMessage)
    LOAD_FUNC(waveInGetNumDevs)
    LOAD_FUNC(waveInGetDevCapsA)
    LOAD_FUNC(waveInGetDevCapsW)
    LOAD_FUNC(waveInGetErrorTextA)
    LOAD_FUNC(waveInGetErrorTextW)
    LOAD_FUNC(waveInOpen)
    LOAD_FUNC(waveInClose)
    LOAD_FUNC(waveInPrepareHeader)
    LOAD_FUNC(waveInUnprepareHeader)
    LOAD_FUNC(waveInAddBuffer)
    LOAD_FUNC(waveInStart)
    LOAD_FUNC(waveInStop)
    LOAD_FUNC(waveInReset)
    LOAD_FUNC(waveInGetPosition)
    LOAD_FUNC(waveInGetID)
    LOAD_FUNC(waveInMessage)
    LOAD_FUNC(midiOutGetNumDevs)
    LOAD_FUNC(midiStreamOpen)
    LOAD_FUNC(midiStreamClose)
    LOAD_FUNC(midiStreamProperty)
    LOAD_FUNC(midiStreamPosition)
    LOAD_FUNC(midiStreamOut)
    LOAD_FUNC(midiStreamPause)
    LOAD_FUNC(midiStreamRestart)
    LOAD_FUNC(midiStreamStop)
    LOAD_FUNC(midiConnect)
    LOAD_FUNC(midiDisconnect)
    LOAD_FUNC(midiOutGetDevCapsA)
    LOAD_FUNC(midiOutGetDevCapsW)
    LOAD_FUNC(midiOutGetVolume)
    LOAD_FUNC(midiOutSetVolume)
    LOAD_FUNC(midiOutGetErrorTextA)
    LOAD_FUNC(midiOutGetErrorTextW)
    LOAD_FUNC(midiOutOpen)
    LOAD_FUNC(midiOutClose)
    LOAD_FUNC(midiOutPrepareHeader)
    LOAD_FUNC(midiOutUnprepareHeader)
    LOAD_FUNC(midiOutShortMsg)
    LOAD_FUNC(midiOutLongMsg)
    LOAD_FUNC(midiOutReset)
    LOAD_FUNC(midiOutCachePatches)
    LOAD_FUNC(midiOutCacheDrumPatches)
    LOAD_FUNC(midiOutGetID)
    LOAD_FUNC(midiOutMessage)
    LOAD_FUNC(midiInGetNumDevs)
    LOAD_FUNC(midiInGetDevCapsA)
    LOAD_FUNC(midiInGetDevCapsW)
    LOAD_FUNC(midiInGetErrorTextA)
    LOAD_FUNC(midiInGetErrorTextW)
    LOAD_FUNC(midiInOpen)
    LOAD_FUNC(midiInClose)
    LOAD_FUNC(midiInPrepareHeader)
    LOAD_FUNC(midiInUnprepareHeader)
    LOAD_FUNC(midiInAddBuffer)
    LOAD_FUNC(midiInStart)
    LOAD_FUNC(midiInStop)
    LOAD_FUNC(midiInReset)
    LOAD_FUNC(midiInGetID)
    LOAD_FUNC(midiInMessage)
    LOAD_FUNC(auxGetNumDevs)
    LOAD_FUNC(auxGetDevCapsA)
    LOAD_FUNC(auxGetDevCapsW)
    LOAD_FUNC(auxSetVolume)
    LOAD_FUNC(auxGetVolume)
    LOAD_FUNC(auxOutMessage)
    LOAD_FUNC(mixerGetNumDevs)
    LOAD_FUNC(mixerGetDevCapsA)
    LOAD_FUNC(mixerGetDevCapsW)
    LOAD_FUNC(mixerOpen)
    LOAD_FUNC(mixerClose)
    LOAD_FUNC(mixerMessage)
    LOAD_FUNC(mixerGetLineInfoA)
    LOAD_FUNC(mixerGetLineInfoW)
    LOAD_FUNC(mixerGetID)
    LOAD_FUNC(mixerGetLineControlsA)
    LOAD_FUNC(mixerGetLineControlsW)
    LOAD_FUNC(mixerGetControlDetailsA)
    LOAD_FUNC(mixerGetControlDetailsW)
    LOAD_FUNC(mixerSetControlDetails)
    LOAD_FUNC(joyGetNumDevs)
    LOAD_FUNC(joyGetDevCapsA)
    LOAD_FUNC(joyGetDevCapsW)
    LOAD_FUNC(joyGetPos)
    LOAD_FUNC(joyGetPosEx)
    LOAD_FUNC(joyGetThreshold)
    LOAD_FUNC(joyReleaseCapture)
    LOAD_FUNC(joySetCapture)
    LOAD_FUNC(joySetThreshold)
    LOAD_FUNC(PlaySoundA)
    LOAD_FUNC(PlaySoundW)
    LOAD_FUNC(sndPlaySoundA)
    LOAD_FUNC(sndPlaySoundW)
    
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
        Log(CLR_CYAN, "       KnC Network Proxy (winmm.dll)");
        Log(CLR_CYAN, "===============================================");
        
        if (!LoadRealWinmm()) {
            Log(CLR_RED, "Failed to load real winmm.dll!");
            return FALSE;
        }
        Log(CLR_GREEN, "Real winmm.dll loaded");
        
        InstallNetworkHooks();
        Log(CLR_CYAN, "-----------------------------------------------");
    }
    else if (reason == DLL_PROCESS_DETACH) {
        if (hWinmm) FreeLibrary(hWinmm);
    }
    return TRUE;
}

// forwarded exports
#define FORWARD(name) extern "C" __declspec(naked) void __stdcall name##_() { __asm { jmp p##name } }

FORWARD(timeGetTime)
FORWARD(timeBeginPeriod)
FORWARD(timeEndPeriod)
FORWARD(timeGetDevCaps)
FORWARD(timeGetSystemTime)
FORWARD(timeSetEvent)
FORWARD(timeKillEvent)
FORWARD(mciSendCommandA)
FORWARD(mciSendCommandW)
FORWARD(mciSendStringA)
FORWARD(mciSendStringW)
FORWARD(mciGetErrorStringA)
FORWARD(mciGetErrorStringW)
FORWARD(mciGetDeviceIDA)
FORWARD(mciGetDeviceIDW)
FORWARD(mciGetDeviceIDFromElementIDA)
FORWARD(mciGetDeviceIDFromElementIDW)
FORWARD(mciExecute)
FORWARD(mciSetYieldProc)
FORWARD(mciGetYieldProc)
FORWARD(mciGetCreatorTask)
FORWARD(mciDriverNotify)
FORWARD(mciDriverYield)
FORWARD(mmioOpenA)
FORWARD(mmioOpenW)
FORWARD(mmioClose)
FORWARD(mmioRead)
FORWARD(mmioWrite)
FORWARD(mmioSeek)
FORWARD(mmioGetInfo)
FORWARD(mmioSetInfo)
FORWARD(mmioSetBuffer)
FORWARD(mmioFlush)
FORWARD(mmioAdvance)
FORWARD(mmioStringToFOURCCA)
FORWARD(mmioStringToFOURCCW)
FORWARD(mmioInstallIOProcA)
FORWARD(mmioInstallIOProcW)
FORWARD(mmioSendMessage)
FORWARD(mmioDescend)
FORWARD(mmioAscend)
FORWARD(mmioCreateChunk)
FORWARD(mmioRenameA)
FORWARD(mmioRenameW)
FORWARD(waveOutGetNumDevs)
FORWARD(waveOutGetDevCapsA)
FORWARD(waveOutGetDevCapsW)
FORWARD(waveOutGetVolume)
FORWARD(waveOutSetVolume)
FORWARD(waveOutGetErrorTextA)
FORWARD(waveOutGetErrorTextW)
FORWARD(waveOutOpen)
FORWARD(waveOutClose)
FORWARD(waveOutPrepareHeader)
FORWARD(waveOutUnprepareHeader)
FORWARD(waveOutWrite)
FORWARD(waveOutPause)
FORWARD(waveOutRestart)
FORWARD(waveOutReset)
FORWARD(waveOutBreakLoop)
FORWARD(waveOutGetPosition)
FORWARD(waveOutGetPitch)
FORWARD(waveOutSetPitch)
FORWARD(waveOutGetPlaybackRate)
FORWARD(waveOutSetPlaybackRate)
FORWARD(waveOutGetID)
FORWARD(waveOutMessage)
FORWARD(waveInGetNumDevs)
FORWARD(waveInGetDevCapsA)
FORWARD(waveInGetDevCapsW)
FORWARD(waveInGetErrorTextA)
FORWARD(waveInGetErrorTextW)
FORWARD(waveInOpen)
FORWARD(waveInClose)
FORWARD(waveInPrepareHeader)
FORWARD(waveInUnprepareHeader)
FORWARD(waveInAddBuffer)
FORWARD(waveInStart)
FORWARD(waveInStop)
FORWARD(waveInReset)
FORWARD(waveInGetPosition)
FORWARD(waveInGetID)
FORWARD(waveInMessage)
FORWARD(midiOutGetNumDevs)
FORWARD(midiStreamOpen)
FORWARD(midiStreamClose)
FORWARD(midiStreamProperty)
FORWARD(midiStreamPosition)
FORWARD(midiStreamOut)
FORWARD(midiStreamPause)
FORWARD(midiStreamRestart)
FORWARD(midiStreamStop)
FORWARD(midiConnect)
FORWARD(midiDisconnect)
FORWARD(midiOutGetDevCapsA)
FORWARD(midiOutGetDevCapsW)
FORWARD(midiOutGetVolume)
FORWARD(midiOutSetVolume)
FORWARD(midiOutGetErrorTextA)
FORWARD(midiOutGetErrorTextW)
FORWARD(midiOutOpen)
FORWARD(midiOutClose)
FORWARD(midiOutPrepareHeader)
FORWARD(midiOutUnprepareHeader)
FORWARD(midiOutShortMsg)
FORWARD(midiOutLongMsg)
FORWARD(midiOutReset)
FORWARD(midiOutCachePatches)
FORWARD(midiOutCacheDrumPatches)
FORWARD(midiOutGetID)
FORWARD(midiOutMessage)
FORWARD(midiInGetNumDevs)
FORWARD(midiInGetDevCapsA)
FORWARD(midiInGetDevCapsW)
FORWARD(midiInGetErrorTextA)
FORWARD(midiInGetErrorTextW)
FORWARD(midiInOpen)
FORWARD(midiInClose)
FORWARD(midiInPrepareHeader)
FORWARD(midiInUnprepareHeader)
FORWARD(midiInAddBuffer)
FORWARD(midiInStart)
FORWARD(midiInStop)
FORWARD(midiInReset)
FORWARD(midiInGetID)
FORWARD(midiInMessage)
FORWARD(auxGetNumDevs)
FORWARD(auxGetDevCapsA)
FORWARD(auxGetDevCapsW)
FORWARD(auxSetVolume)
FORWARD(auxGetVolume)
FORWARD(auxOutMessage)
FORWARD(mixerGetNumDevs)
FORWARD(mixerGetDevCapsA)
FORWARD(mixerGetDevCapsW)
FORWARD(mixerOpen)
FORWARD(mixerClose)
FORWARD(mixerMessage)
FORWARD(mixerGetLineInfoA)
FORWARD(mixerGetLineInfoW)
FORWARD(mixerGetID)
FORWARD(mixerGetLineControlsA)
FORWARD(mixerGetLineControlsW)
FORWARD(mixerGetControlDetailsA)
FORWARD(mixerGetControlDetailsW)
FORWARD(mixerSetControlDetails)
FORWARD(joyGetNumDevs)
FORWARD(joyGetDevCapsA)
FORWARD(joyGetDevCapsW)
FORWARD(joyGetPos)
FORWARD(joyGetPosEx)
FORWARD(joyGetThreshold)
FORWARD(joyReleaseCapture)
FORWARD(joySetCapture)
FORWARD(joySetThreshold)
FORWARD(PlaySoundA)
FORWARD(PlaySoundW)
FORWARD(sndPlaySoundA)
FORWARD(sndPlaySoundW)

