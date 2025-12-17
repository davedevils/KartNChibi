// ws2_32.dll proxy - full replacement approach
// loads real dll from system32 and forwards all calls

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdio>
#include <cstdint>
#include <ctime>

#pragma comment(lib, "ws2_32.lib")

// real dll handle and function pointers
HINSTANCE hL = nullptr;
FARPROC p[195] = { 0 };

// console colors
#define CLR_RESET   "\033[0m"
#define CLR_RED     "\033[91m"
#define CLR_GREEN   "\033[92m"
#define CLR_YELLOW  "\033[93m"
#define CLR_CYAN    "\033[96m"

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
    
    // hex dump first 32 bytes
    char hex[256] = {0};
    int n = len > 32 ? 32 : len;
    for (int i = 0; i < n; i++) {
        sprintf(hex + i*3, "%02X ", (uint8_t)buf[i]);
    }
    Log(color, "  %s", hex);
}

// hooked send
extern "C" int WSAAPI hook_send(SOCKET s, const char* buf, int len, int flags) {
    typedef int (WSAAPI* fn)(SOCKET, const char*, int, int);
    LogPacket(true, buf, len);
    return ((fn)p[190])(s, buf, len, flags);
}

// hooked recv
extern "C" int WSAAPI hook_recv(SOCKET s, char* buf, int len, int flags) {
    typedef int (WSAAPI* fn)(SOCKET, char*, int, int);
    int result = ((fn)p[187])(s, buf, len, flags);
    if (result > 0) {
        LogPacket(false, buf, result);
    }
    return result;
}

// hooked connect
extern "C" int WSAAPI hook_connect(SOCKET s, const struct sockaddr* name, int namelen) {
    typedef int (WSAAPI* fn)(SOCKET, const struct sockaddr*, int);
    
    if (name->sa_family == AF_INET) {
        struct sockaddr_in* addr = (struct sockaddr_in*)name;
        char ip[32];
        inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip));
        Log(CLR_CYAN, "===============================================");
        Log(CLR_CYAN, "  CONNECT TO %s:%d", ip, ntohs(addr->sin_port));
        Log(CLR_CYAN, "===============================================");
    }
    
    return ((fn)p[163])(s, name, namelen);
}

bool LoadRealDLL() {
    char systemPath[MAX_PATH];
    GetSystemDirectoryA(systemPath, MAX_PATH);
    strcat_s(systemPath, "\\WS2_32.dll");
    
    hL = LoadLibraryA(systemPath);
    if (!hL) {
        Log(CLR_RED, "Failed to load real WS2_32.dll!");
        return false;
    }
    
    // load all functions
    p[0] = GetProcAddress(hL, "FreeAddrInfoEx");
    p[1] = GetProcAddress(hL, "FreeAddrInfoExW");
    p[2] = GetProcAddress(hL, "FreeAddrInfoW");
    p[3] = GetProcAddress(hL, "GetAddrInfoExA");
    p[4] = GetProcAddress(hL, "GetAddrInfoExCancel");
    p[5] = GetProcAddress(hL, "GetAddrInfoExOverlappedResult");
    p[6] = GetProcAddress(hL, "GetAddrInfoExW");
    p[7] = GetProcAddress(hL, "GetAddrInfoW");
    p[8] = GetProcAddress(hL, "GetHostNameW");
    p[9] = GetProcAddress(hL, "GetNameInfoW");
    p[10] = GetProcAddress(hL, "InetNtopW");
    p[11] = GetProcAddress(hL, "InetPtonW");
    p[12] = GetProcAddress(hL, "SetAddrInfoExA");
    p[13] = GetProcAddress(hL, "SetAddrInfoExW");
    p[14] = GetProcAddress(hL, "WEP");
    p[15] = GetProcAddress(hL, "WPUCompleteOverlappedRequest");
    p[16] = GetProcAddress(hL, "WPUGetProviderPathEx");
    p[17] = GetProcAddress(hL, "WSAAccept");
    p[18] = GetProcAddress(hL, "WSAAddressToStringA");
    p[19] = GetProcAddress(hL, "WSAAddressToStringW");
    p[20] = GetProcAddress(hL, "WSAAdvertiseProvider");
    p[21] = GetProcAddress(hL, "WSAAsyncGetHostByAddr");
    p[22] = GetProcAddress(hL, "WSAAsyncGetHostByName");
    p[23] = GetProcAddress(hL, "WSAAsyncGetProtoByName");
    p[24] = GetProcAddress(hL, "WSAAsyncGetProtoByNumber");
    p[25] = GetProcAddress(hL, "WSAAsyncGetServByName");
    p[26] = GetProcAddress(hL, "WSAAsyncGetServByPort");
    p[27] = GetProcAddress(hL, "WSAAsyncSelect");
    p[28] = GetProcAddress(hL, "WSACancelAsyncRequest");
    p[29] = GetProcAddress(hL, "WSACancelBlockingCall");
    p[30] = GetProcAddress(hL, "WSACleanup");
    p[31] = GetProcAddress(hL, "WSACloseEvent");
    p[32] = GetProcAddress(hL, "WSAConnect");
    p[33] = GetProcAddress(hL, "WSAConnectByList");
    p[34] = GetProcAddress(hL, "WSAConnectByNameA");
    p[35] = GetProcAddress(hL, "WSAConnectByNameW");
    p[36] = GetProcAddress(hL, "WSACreateEvent");
    p[37] = GetProcAddress(hL, "WSADuplicateSocketA");
    p[38] = GetProcAddress(hL, "WSADuplicateSocketW");
    p[39] = GetProcAddress(hL, "WSAEnumNameSpaceProvidersA");
    p[40] = GetProcAddress(hL, "WSAEnumNameSpaceProvidersExA");
    p[41] = GetProcAddress(hL, "WSAEnumNameSpaceProvidersExW");
    p[42] = GetProcAddress(hL, "WSAEnumNameSpaceProvidersW");
    p[43] = GetProcAddress(hL, "WSAEnumNetworkEvents");
    p[44] = GetProcAddress(hL, "WSAEnumProtocolsA");
    p[45] = GetProcAddress(hL, "WSAEnumProtocolsW");
    p[46] = GetProcAddress(hL, "WSAEventSelect");
    p[47] = GetProcAddress(hL, "WSAGetLastError");
    p[48] = GetProcAddress(hL, "WSAGetOverlappedResult");
    p[49] = GetProcAddress(hL, "WSAGetQOSByName");
    p[50] = GetProcAddress(hL, "WSAGetServiceClassInfoA");
    p[51] = GetProcAddress(hL, "WSAGetServiceClassInfoW");
    p[52] = GetProcAddress(hL, "WSAGetServiceClassNameByClassIdA");
    p[53] = GetProcAddress(hL, "WSAGetServiceClassNameByClassIdW");
    p[54] = GetProcAddress(hL, "WSAHtonl");
    p[55] = GetProcAddress(hL, "WSAHtons");
    p[56] = GetProcAddress(hL, "WSAInstallServiceClassA");
    p[57] = GetProcAddress(hL, "WSAInstallServiceClassW");
    p[58] = GetProcAddress(hL, "WSAIoctl");
    p[59] = GetProcAddress(hL, "WSAIsBlocking");
    p[60] = GetProcAddress(hL, "WSAJoinLeaf");
    p[61] = GetProcAddress(hL, "WSALookupServiceBeginA");
    p[62] = GetProcAddress(hL, "WSALookupServiceBeginW");
    p[63] = GetProcAddress(hL, "WSALookupServiceEnd");
    p[64] = GetProcAddress(hL, "WSALookupServiceNextA");
    p[65] = GetProcAddress(hL, "WSALookupServiceNextW");
    p[66] = GetProcAddress(hL, "WSANSPIoctl");
    p[67] = GetProcAddress(hL, "WSANtohl");
    p[68] = GetProcAddress(hL, "WSANtohs");
    p[69] = GetProcAddress(hL, "WSAPoll");
    p[70] = GetProcAddress(hL, "WSAProviderCompleteAsyncCall");
    p[71] = GetProcAddress(hL, "WSAProviderConfigChange");
    p[72] = GetProcAddress(hL, "WSARecv");
    p[73] = GetProcAddress(hL, "WSARecvDisconnect");
    p[74] = GetProcAddress(hL, "WSARecvFrom");
    p[75] = GetProcAddress(hL, "WSARemoveServiceClass");
    p[76] = GetProcAddress(hL, "WSAResetEvent");
    p[77] = GetProcAddress(hL, "WSASend");
    p[78] = GetProcAddress(hL, "WSASendDisconnect");
    p[79] = GetProcAddress(hL, "WSASendMsg");
    p[80] = GetProcAddress(hL, "WSASendTo");
    p[81] = GetProcAddress(hL, "WSASetBlockingHook");
    p[82] = GetProcAddress(hL, "WSASetEvent");
    p[83] = GetProcAddress(hL, "WSASetLastError");
    p[84] = GetProcAddress(hL, "WSASetServiceA");
    p[85] = GetProcAddress(hL, "WSASetServiceW");
    p[86] = GetProcAddress(hL, "WSASocketA");
    p[87] = GetProcAddress(hL, "WSASocketW");
    p[88] = GetProcAddress(hL, "WSAStartup");
    p[89] = GetProcAddress(hL, "WSAStringToAddressA");
    p[90] = GetProcAddress(hL, "WSAStringToAddressW");
    p[91] = GetProcAddress(hL, "WSAUnadvertiseProvider");
    p[92] = GetProcAddress(hL, "WSAUnhookBlockingHook");
    p[93] = GetProcAddress(hL, "WSAWaitForMultipleEvents");
    p[94] = GetProcAddress(hL, "WSApSetPostRoutine");
    p[95] = GetProcAddress(hL, "WSCDeinstallProvider");
    p[96] = GetProcAddress(hL, "WSCDeinstallProvider32");
    p[97] = GetProcAddress(hL, "WSCDeinstallProviderEx");
    p[98] = GetProcAddress(hL, "WSCEnableNSProvider");
    p[99] = GetProcAddress(hL, "WSCEnableNSProvider32");
    p[100] = GetProcAddress(hL, "WSCEnumNameSpaceProviders32");
    p[101] = GetProcAddress(hL, "WSCEnumNameSpaceProvidersEx32");
    p[102] = GetProcAddress(hL, "WSCEnumProtocols");
    p[103] = GetProcAddress(hL, "WSCEnumProtocols32");
    p[104] = GetProcAddress(hL, "WSCEnumProtocolsEx");
    p[105] = GetProcAddress(hL, "WSCGetApplicationCategory");
    p[106] = GetProcAddress(hL, "WSCGetApplicationCategoryEx");
    p[107] = GetProcAddress(hL, "WSCGetProviderInfo");
    p[108] = GetProcAddress(hL, "WSCGetProviderInfo32");
    p[109] = GetProcAddress(hL, "WSCGetProviderPath");
    p[110] = GetProcAddress(hL, "WSCGetProviderPath32");
    p[111] = GetProcAddress(hL, "WSCInstallNameSpace");
    p[112] = GetProcAddress(hL, "WSCInstallNameSpace32");
    p[113] = GetProcAddress(hL, "WSCInstallNameSpaceEx");
    p[114] = GetProcAddress(hL, "WSCInstallNameSpaceEx2");
    p[115] = GetProcAddress(hL, "WSCInstallNameSpaceEx32");
    p[116] = GetProcAddress(hL, "WSCInstallProvider");
    p[117] = GetProcAddress(hL, "WSCInstallProvider64_32");
    p[118] = GetProcAddress(hL, "WSCInstallProviderAndChains64_32");
    p[119] = GetProcAddress(hL, "WSCInstallProviderEx");
    p[120] = GetProcAddress(hL, "WSCSetApplicationCategory");
    p[121] = GetProcAddress(hL, "WSCSetApplicationCategoryEx");
    p[122] = GetProcAddress(hL, "WSCSetProviderInfo");
    p[123] = GetProcAddress(hL, "WSCSetProviderInfo32");
    p[124] = GetProcAddress(hL, "WSCUnInstallNameSpace");
    p[125] = GetProcAddress(hL, "WSCUnInstallNameSpace32");
    p[126] = GetProcAddress(hL, "WSCUnInstallNameSpaceEx2");
    p[127] = GetProcAddress(hL, "WSCUpdateProvider");
    p[128] = GetProcAddress(hL, "WSCUpdateProvider32");
    p[129] = GetProcAddress(hL, "WSCUpdateProviderEx");
    p[130] = GetProcAddress(hL, "WSCWriteNameSpaceOrder");
    p[131] = GetProcAddress(hL, "WSCWriteNameSpaceOrder32");
    p[132] = GetProcAddress(hL, "WSCWriteProviderOrder");
    p[133] = GetProcAddress(hL, "WSCWriteProviderOrder32");
    p[134] = GetProcAddress(hL, "WSCWriteProviderOrderEx");
    p[135] = GetProcAddress(hL, "WahCloseApcHelper");
    p[136] = GetProcAddress(hL, "WahCloseHandleHelper");
    p[137] = GetProcAddress(hL, "WahCloseNotificationHandleHelper");
    p[138] = GetProcAddress(hL, "WahCloseSocketHandle");
    p[139] = GetProcAddress(hL, "WahCloseThread");
    p[140] = GetProcAddress(hL, "WahCompleteRequest");
    p[141] = GetProcAddress(hL, "WahCreateHandleContextTable");
    p[142] = GetProcAddress(hL, "WahCreateNotificationHandle");
    p[143] = GetProcAddress(hL, "WahCreateSocketHandle");
    p[144] = GetProcAddress(hL, "WahDestroyHandleContextTable");
    p[145] = GetProcAddress(hL, "WahDisableNonIFSHandleSupport");
    p[146] = GetProcAddress(hL, "WahEnableNonIFSHandleSupport");
    p[147] = GetProcAddress(hL, "WahEnumerateHandleContexts");
    p[148] = GetProcAddress(hL, "WahInsertHandleContext");
    p[149] = GetProcAddress(hL, "WahNotifyAllProcesses");
    p[150] = GetProcAddress(hL, "WahOpenApcHelper");
    p[151] = GetProcAddress(hL, "WahOpenCurrentThread");
    p[152] = GetProcAddress(hL, "WahOpenHandleHelper");
    p[153] = GetProcAddress(hL, "WahOpenNotificationHandleHelper");
    p[154] = GetProcAddress(hL, "WahQueueUserApc");
    p[155] = GetProcAddress(hL, "WahReferenceContextByHandle");
    p[156] = GetProcAddress(hL, "WahRemoveHandleContext");
    p[157] = GetProcAddress(hL, "WahWaitForNotification");
    p[158] = GetProcAddress(hL, "WahWriteLSPEvent");
    p[159] = GetProcAddress(hL, "__WSAFDIsSet");
    p[160] = GetProcAddress(hL, "accept");
    p[161] = GetProcAddress(hL, "bind");
    p[162] = GetProcAddress(hL, "closesocket");
    p[163] = GetProcAddress(hL, "connect");
    p[164] = GetProcAddress(hL, "freeaddrinfo");
    p[165] = GetProcAddress(hL, "getaddrinfo");
    p[166] = GetProcAddress(hL, "gethostbyaddr");
    p[167] = GetProcAddress(hL, "gethostbyname");
    p[168] = GetProcAddress(hL, "gethostname");
    p[169] = GetProcAddress(hL, "getnameinfo");
    p[170] = GetProcAddress(hL, "getpeername");
    p[171] = GetProcAddress(hL, "getprotobyname");
    p[172] = GetProcAddress(hL, "getprotobynumber");
    p[173] = GetProcAddress(hL, "getservbyname");
    p[174] = GetProcAddress(hL, "getservbyport");
    p[175] = GetProcAddress(hL, "getsockname");
    p[176] = GetProcAddress(hL, "getsockopt");
    p[177] = GetProcAddress(hL, "htonl");
    p[178] = GetProcAddress(hL, "htons");
    p[179] = GetProcAddress(hL, "inet_addr");
    p[180] = GetProcAddress(hL, "inet_ntoa");
    p[181] = GetProcAddress(hL, "inet_ntop");
    p[182] = GetProcAddress(hL, "inet_pton");
    p[183] = GetProcAddress(hL, "ioctlsocket");
    p[184] = GetProcAddress(hL, "listen");
    p[185] = GetProcAddress(hL, "ntohl");
    p[186] = GetProcAddress(hL, "ntohs");
    p[187] = GetProcAddress(hL, "recv");
    p[188] = GetProcAddress(hL, "recvfrom");
    p[189] = GetProcAddress(hL, "select");
    p[190] = GetProcAddress(hL, "send");
    p[191] = GetProcAddress(hL, "sendto");
    p[192] = GetProcAddress(hL, "setsockopt");
    p[193] = GetProcAddress(hL, "shutdown");
    p[194] = GetProcAddress(hL, "socket");
    
    Log(CLR_GREEN, "Real WS2_32.dll loaded from %s", systemPath);
    return true;
}

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        AllocConsole();
        freopen("CONOUT$", "w", stdout);
        
        // enable ANSI colors
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode;
        GetConsoleMode(hOut, &mode);
        SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        
        Log(CLR_CYAN, "===============================================");
        Log(CLR_CYAN, "       KnC Network Proxy - WS2_32.dll");
        Log(CLR_CYAN, "===============================================");
        
        if (!LoadRealDLL()) {
            return FALSE;
        }
        
        Log(CLR_GREEN, "Proxy ready - send/recv/connect hooked");
        Log(CLR_CYAN, "-----------------------------------------------");
    }
    else if (reason == DLL_PROCESS_DETACH) {
        if (hL) FreeLibrary(hL);
    }
    return TRUE;
}

// all forwarded functions using naked asm jumps
#define FORWARD(idx) extern "C" __declspec(naked) void __stdcall __E__##idx##__() { __asm { jmp p[idx * 4] } }

FORWARD(0)   // FreeAddrInfoEx
FORWARD(1)   // FreeAddrInfoExW
FORWARD(2)   // FreeAddrInfoW
FORWARD(3)   // GetAddrInfoExA
FORWARD(4)   // GetAddrInfoExCancel
FORWARD(5)   // GetAddrInfoExOverlappedResult
FORWARD(6)   // GetAddrInfoExW
FORWARD(7)   // GetAddrInfoW
FORWARD(8)   // GetHostNameW
FORWARD(9)   // GetNameInfoW
FORWARD(10)  // InetNtopW
FORWARD(11)  // InetPtonW
FORWARD(12)  // SetAddrInfoExA
FORWARD(13)  // SetAddrInfoExW
FORWARD(14)  // WEP
FORWARD(15)  // WPUCompleteOverlappedRequest
FORWARD(16)  // WPUGetProviderPathEx
FORWARD(17)  // WSAAccept
FORWARD(18)  // WSAAddressToStringA
FORWARD(19)  // WSAAddressToStringW
FORWARD(20)  // WSAAdvertiseProvider
FORWARD(21)  // WSAAsyncGetHostByAddr
FORWARD(22)  // WSAAsyncGetHostByName
FORWARD(23)  // WSAAsyncGetProtoByName
FORWARD(24)  // WSAAsyncGetProtoByNumber
FORWARD(25)  // WSAAsyncGetServByName
FORWARD(26)  // WSAAsyncGetServByPort
FORWARD(27)  // WSAAsyncSelect
FORWARD(28)  // WSACancelAsyncRequest
FORWARD(29)  // WSACancelBlockingCall
FORWARD(30)  // WSACleanup
FORWARD(31)  // WSACloseEvent
FORWARD(32)  // WSAConnect
FORWARD(33)  // WSAConnectByList
FORWARD(34)  // WSAConnectByNameA
FORWARD(35)  // WSAConnectByNameW
FORWARD(36)  // WSACreateEvent
FORWARD(37)  // WSADuplicateSocketA
FORWARD(38)  // WSADuplicateSocketW
FORWARD(39)  // WSAEnumNameSpaceProvidersA
FORWARD(40)  // WSAEnumNameSpaceProvidersExA
FORWARD(41)  // WSAEnumNameSpaceProvidersExW
FORWARD(42)  // WSAEnumNameSpaceProvidersW
FORWARD(43)  // WSAEnumNetworkEvents
FORWARD(44)  // WSAEnumProtocolsA
FORWARD(45)  // WSAEnumProtocolsW
FORWARD(46)  // WSAEventSelect
FORWARD(47)  // WSAGetLastError
FORWARD(48)  // WSAGetOverlappedResult
FORWARD(49)  // WSAGetQOSByName
FORWARD(50)  // WSAGetServiceClassInfoA
FORWARD(51)  // WSAGetServiceClassInfoW
FORWARD(52)  // WSAGetServiceClassNameByClassIdA
FORWARD(53)  // WSAGetServiceClassNameByClassIdW
FORWARD(54)  // WSAHtonl
FORWARD(55)  // WSAHtons
FORWARD(56)  // WSAInstallServiceClassA
FORWARD(57)  // WSAInstallServiceClassW
FORWARD(58)  // WSAIoctl
FORWARD(59)  // WSAIsBlocking
FORWARD(60)  // WSAJoinLeaf
FORWARD(61)  // WSALookupServiceBeginA
FORWARD(62)  // WSALookupServiceBeginW
FORWARD(63)  // WSALookupServiceEnd
FORWARD(64)  // WSALookupServiceNextA
FORWARD(65)  // WSALookupServiceNextW
FORWARD(66)  // WSANSPIoctl
FORWARD(67)  // WSANtohl
FORWARD(68)  // WSANtohs
FORWARD(69)  // WSAPoll
FORWARD(70)  // WSAProviderCompleteAsyncCall
FORWARD(71)  // WSAProviderConfigChange
FORWARD(72)  // WSARecv
FORWARD(73)  // WSARecvDisconnect
FORWARD(74)  // WSARecvFrom
FORWARD(75)  // WSARemoveServiceClass
FORWARD(76)  // WSAResetEvent
FORWARD(77)  // WSASend
FORWARD(78)  // WSASendDisconnect
FORWARD(79)  // WSASendMsg
FORWARD(80)  // WSASendTo
FORWARD(81)  // WSASetBlockingHook
FORWARD(82)  // WSASetEvent
FORWARD(83)  // WSASetLastError
FORWARD(84)  // WSASetServiceA
FORWARD(85)  // WSASetServiceW
FORWARD(86)  // WSASocketA
FORWARD(87)  // WSASocketW
FORWARD(88)  // WSAStartup
FORWARD(89)  // WSAStringToAddressA
FORWARD(90)  // WSAStringToAddressW
FORWARD(91)  // WSAUnadvertiseProvider
FORWARD(92)  // WSAUnhookBlockingHook
FORWARD(93)  // WSAWaitForMultipleEvents
FORWARD(94)  // WSApSetPostRoutine
FORWARD(95)  // WSCDeinstallProvider
FORWARD(96)  // WSCDeinstallProvider32
FORWARD(97)  // WSCDeinstallProviderEx
FORWARD(98)  // WSCEnableNSProvider
FORWARD(99)  // WSCEnableNSProvider32
FORWARD(100) // WSCEnumNameSpaceProviders32
FORWARD(101) // WSCEnumNameSpaceProvidersEx32
FORWARD(102) // WSCEnumProtocols
FORWARD(103) // WSCEnumProtocols32
FORWARD(104) // WSCEnumProtocolsEx
FORWARD(105) // WSCGetApplicationCategory
FORWARD(106) // WSCGetApplicationCategoryEx
FORWARD(107) // WSCGetProviderInfo
FORWARD(108) // WSCGetProviderInfo32
FORWARD(109) // WSCGetProviderPath
FORWARD(110) // WSCGetProviderPath32
FORWARD(111) // WSCInstallNameSpace
FORWARD(112) // WSCInstallNameSpace32
FORWARD(113) // WSCInstallNameSpaceEx
FORWARD(114) // WSCInstallNameSpaceEx2
FORWARD(115) // WSCInstallNameSpaceEx32
FORWARD(116) // WSCInstallProvider
FORWARD(117) // WSCInstallProvider64_32
FORWARD(118) // WSCInstallProviderAndChains64_32
FORWARD(119) // WSCInstallProviderEx
FORWARD(120) // WSCSetApplicationCategory
FORWARD(121) // WSCSetApplicationCategoryEx
FORWARD(122) // WSCSetProviderInfo
FORWARD(123) // WSCSetProviderInfo32
FORWARD(124) // WSCUnInstallNameSpace
FORWARD(125) // WSCUnInstallNameSpace32
FORWARD(126) // WSCUnInstallNameSpaceEx2
FORWARD(127) // WSCUpdateProvider
FORWARD(128) // WSCUpdateProvider32
FORWARD(129) // WSCUpdateProviderEx
FORWARD(130) // WSCWriteNameSpaceOrder
FORWARD(131) // WSCWriteNameSpaceOrder32
FORWARD(132) // WSCWriteProviderOrder
FORWARD(133) // WSCWriteProviderOrder32
FORWARD(134) // WSCWriteProviderOrderEx
FORWARD(135) // WahCloseApcHelper
FORWARD(136) // WahCloseHandleHelper
FORWARD(137) // WahCloseNotificationHandleHelper
FORWARD(138) // WahCloseSocketHandle
FORWARD(139) // WahCloseThread
FORWARD(140) // WahCompleteRequest
FORWARD(141) // WahCreateHandleContextTable
FORWARD(142) // WahCreateNotificationHandle
FORWARD(143) // WahCreateSocketHandle
FORWARD(144) // WahDestroyHandleContextTable
FORWARD(145) // WahDisableNonIFSHandleSupport
FORWARD(146) // WahEnableNonIFSHandleSupport
FORWARD(147) // WahEnumerateHandleContexts
FORWARD(148) // WahInsertHandleContext
FORWARD(149) // WahNotifyAllProcesses
FORWARD(150) // WahOpenApcHelper
FORWARD(151) // WahOpenCurrentThread
FORWARD(152) // WahOpenHandleHelper
FORWARD(153) // WahOpenNotificationHandleHelper
FORWARD(154) // WahQueueUserApc
FORWARD(155) // WahReferenceContextByHandle
FORWARD(156) // WahRemoveHandleContext
FORWARD(157) // WahWaitForNotification
FORWARD(158) // WahWriteLSPEvent
FORWARD(159) // __WSAFDIsSet
FORWARD(160) // accept
FORWARD(161) // bind
FORWARD(162) // closesocket
// 163 = connect - hooked
FORWARD(164) // freeaddrinfo
FORWARD(165) // getaddrinfo
FORWARD(166) // gethostbyaddr
FORWARD(167) // gethostbyname
FORWARD(168) // gethostname
FORWARD(169) // getnameinfo
FORWARD(170) // getpeername
FORWARD(171) // getprotobyname
FORWARD(172) // getprotobynumber
FORWARD(173) // getservbyname
FORWARD(174) // getservbyport
FORWARD(175) // getsockname
FORWARD(176) // getsockopt
FORWARD(177) // htonl
FORWARD(178) // htons
FORWARD(179) // inet_addr
FORWARD(180) // inet_ntoa
FORWARD(181) // inet_ntop
FORWARD(182) // inet_pton
FORWARD(183) // ioctlsocket
FORWARD(184) // listen
FORWARD(185) // ntohl
FORWARD(186) // ntohs
// 187 = recv - hooked
FORWARD(188) // recvfrom
FORWARD(189) // select
// 190 = send - hooked
FORWARD(191) // sendto
FORWARD(192) // setsockopt
FORWARD(193) // shutdown
FORWARD(194) // socket

