# GOA - Network Stack

All addresses from `KnC_dump_SCY.exe`, image base `0x400000`, x86 MSVC 7.1.

## Network singleton

| Element | Address/Value |
|---|---|
| Instance | `0x862000` (`.data0`) |
| Primary vtable | `0x5301CC` - `[dtor, Init, Close, Dispatch]` |
| Secondary vtable (MI) | `0x5301C8` |
| Class size | ~2.5 MB (5960-slot x 32 B table + buffers) |
| Second base offset | `+536096` bytes |
| Constructor | `sub_43F6D0` (536 B) |
| Destructor | `sub_43F8F0` |

### Vtable

| Slot | Function | Role |
|---|---|---|
| 0 | `sub_43F8F0` | dtor |
| 1 | `sub_43E470` | `Init(int sock, const char* ip, int flags)` |
| 2 | `sub_43E9A0` | `Close()` |
| 3 | `sub_43EBA0` | `Dispatch(CPacket*)` - main opcode switch |

## Socket wrappers

Direct WS2_32 IAT calls. NOT guarded by anti-hook sentinels.

| Function | Size | Role | API calls |
|---|---|---|---|
| `sub_43DE50` | 93 | `WSAStartup(0x202)` + version check | `WSAStartup`, `WSACleanup` |
| `sub_43DF50` | 142 | Socket options | `setsockopt` x3, `ioctlsocket` |
| `sub_43E470` | 106 | `Init` - store sock/ip/flags, call `sub_43DF50` then `sub_43DFE0` | - |
| `sub_43E530` | 348 | **Connect** - create + connect + select (1s timeout) | `WSASocketA`, `connect`, `inet_addr`, `htons`, `select`, `__WSAFDIsSet`, `closesocket`, `shutdown` |
| `sub_43E8B0` | 47 | `Startup()` - WSAStartup wrapper + init | - |
| `sub_43E8F0` | 175 | `Connect(ip, port)` - calls `sub_43E530` + creates event handle | - |
| `sub_43E410` | 93 | Close: `shutdown` + `closesocket` | - |
| `sub_43E4E0` | 75 | Close variant (vtable slot 2) | - |
| `sub_43E8E0` | 11 | `WSACleanup` stub | - |

### Socket options (`sub_43DF50`)

| Level | Option | Value | Effect |
|---|---|---|---|
| `SOL_SOCKET` | `SO_KEEPALIVE` | 1 | Keep-alive on |
| `SOL_SOCKET` | `SO_DONTROUTE` | 1 | Bypass routing |
| - | `FIONBIO` (ioctlsocket) | 1 | Non-blocking |
| `IPPROTO_TCP` | `TCP_NODELAY` | 0 | Nagle ON |

No IOCP, no `WSAAsyncSelect`, no `WSAEventSelect`. Poll via `select`.

### Connect flow (`sub_43E530`)

```
1. If sock != INVALID_SOCKET: shutdown + closesocket
2. WSASocketA(AF_INET, SOCK_STREAM, 0, 0, 0, WSA_FLAG_OVERLAPPED)
3. Build sockaddr_in: inet_addr(cp) + htons(port)
4. connect(sock, &addr, 16) // non-blocking -> WSAEWOULDBLOCK
5. select(0, NULL, &writefds, NULL, {1s, 0us})
6. !WSAFDIsSet -> closesocket, return 0 (timeout)
7. Return 1 (connected)
```

### Connect callers

| Caller | Role |
|---|---|
| `sub_403C20` (x2) | Main ConnectToServer. Reads IP from `this+329`, port from `this+584`. Failure: `MSG_SERVER_NOT_READY` / `MSG_CONNECT_FAIL`. Sleep 1000ms then `sub_4467F0`. |
| `sub_4182B0` | Alternate connect path |
| `sub_42FDE0` (x2) | Reconnect with two-step retry |
| `sub_441D10` | Mid-session server switch (dispatcher case 83) |

## Wire format

```
[size u16 LE][opcode u16 LE][payload ...]
```

4-byte header. Total wire bytes = payload + 4. Confirmed from `sub_428FF0` and `sub_43E110`.

NOT the same as modern 8-byte header.

### CPacket layout

```c
struct CPacket {
 void* vtable; // +0 -> &off_52AED0
 uint16* opcode_ptr; // +4 -> buffer[+2]
 uint16* size_ptr; // +8 -> buffer[+0]
 uint16 size_field; // +12 = first 2 bytes
 uint16 opcode_field; // +14 = next 2 bytes
 uint8 payload[2048]; // +16 = payload start
 // cursor state at +2060..+2084
};
```

Cursor offsets (set by `sub_428FF0`):

| Offset | Field | Role |
|---|---|---|
| `+2060` | `read_base` | `this + 16` |
| `+2064` | `read_cursor` | advances on ReadBytes |
| `+2068` | `write_cursor` | advances on WriteBytes |
| `+2072` | `read_end` | `this + 16 + buffer_size` |
| `+2080` | - | internal mirror |
| `+2084` | - | internal mirror |

### Reader API

| Function | Role |
|---|---|
| `sub_428D70` | `GetOpcode()` - `*(WORD*)opcode_ptr` |
| `sub_428D80` | `GetSize()` - `*(WORD*)size_ptr` |
| `sub_428D90` | `ReadBytes(dst, len)` - bounds-checked, 317 xrefs |
| `sub_428F40` | `ReadString(char*)` - null-terminated ASCII |
| `sub_428F70` | `ReadWString(wchar_t*)` - null-terminated UTF-16LE |

### Writer API

| Function | Role |
|---|---|
| `sub_4290E0` | `PacketBegin(opcode)` - init vtable, alloc 2048B, stamp opcode |
| `sub_428E10` | `WriteBytes(src, len)` - increments size at `+12` |
| `sub_428EE0` | `WriteString(src)` - ASCII + null |
| `sub_428F10` | `WriteWString(src)` - UTF-16LE + null |
| `sub_43E110` | `Enqueue(pkt)` - push to send queue (cap 1024) |
| `sub_428D60` | `PacketDestroy(pkt)` - reset vtable |

Dispatcher subtracts 1 from opcode before switch. Matches modern `cmd - 1` convention.

## send/WSASend anti-hook sentinels

`send` and `WSASend` IAT slots are NEVER called by packet code. Only referenced by sentinel functions:

| Function | Address | Role |
|---|---|---|
| `?InitSocketAddr@@YAKXZ` | `0x482D10` | Startup: resolve real `send`/`WSASend` via custom `GetProcAddress` (`sub_483370`), store in `dword_BE57F4`/`dword_BE57F8`. Capture IAT baseline in `dword_BE5808`/`dword_BE580C`. |
| `?CheckSocketAddr@@YAKXZ` | `0x482E60` | Periodic: compare IAT slots vs baseline. Mismatch -> increment error counter, report via `sub_481880`. |

### Custom GetProcAddress (`sub_483370`)

829 bytes. Manually parses PE export directory without calling `GetProcAddress`. Used to resolve WS2_32 functions independent of IAT.

Callers:
- `?InitNPSC@CGameGuard@@QAE_NXZ` - nProtect GameGuard
- `?InitToolhelp32@@YA_NXZ` - `CreateToolhelp32Snapshot` etc
- `?InitTimeAddr@@YAKXZ` - sentinel on `timeGetTime`/`GetTickCount`
- `?InitSocketAddr@@YAKXZ`
- `?ReadMemCrc@@YAKAAU_PROCESS_INFORMATION@@@Z` - process memory CRC
- `?CheckVersion@@YA_NPADGGGG@Z`
- `?CheckRsaBase@@YA_NXZ` - RSA code section check (2700 B)

### String decryptor `_Dect1` (`?_Dect1@@YAPADPAD@Z`)

451 bytes, **208 callers**. All anti-cheat/crypto/sentinel strings stored encrypted, decrypted on-the-fly.

```
if ciphertext[0] == 1 { // encrypted marker
 len_key = 3 * ciphertext[1] + 101
 ciphertext[2] ^= len_key // decrypt length
 kstate = 9 * ciphertext[1] + 3
 ciphertext[3] ^= (kstate + 101) // decrypt first key byte
 kstate += 1
 for i in 0..length {
 kstate *= 3
 ciphertext[i] ^= (kstate + 101)
 kstate += 1
 }
 ciphertext[length] = 0
}
```

Re-encrypts previous string when new one requested. Anti-RAM-scraper: cleartext only during use window.

### Actual send path

`send`/`WSASend` NOT from IAT. Resolved at runtime:
```c
extern int (*resolved_send)(SOCKET, const char*, int, int);
resolved_send(sock, buf, len, 0);
```

Populated by `sub_483370` from ws2_32.dll export table. Same for `recv`/`WSARecv`.

Read/write loop: `sub_47F690` (8679 bytes). References `dword_BE57DC` (anti-cheat state) ~7 times. Bundles recv loop + anti-cheat state machine + crypto hooks. Detailed analysis deferred.

## Dispatcher (`sub_43EBA0`)

Vtable slot 3. Switch on `GetOpcode() - 1`, ~160 handlers.

```c
int Dispatch(CPacket* pkt) {
 switch (pkt->GetOpcode() - 1) {
 case 0: return sub_440230(pkt); // opcode 1
 case 1: return sub_4401D0(pkt); // opcode 2
 ...
 case 200: return sub_443780(pkt); // opcode 201
 }
}
```

200 switch cases: ~127 real handlers, 28 route to `nullsub_2`.

### Ignored opcodes (wire values)

```
31, 32, 36, 54, 55, 56, 67, 72, 101, 109, 111, 113,
120, 121, 122, 128, 129, 131, 132, 133, 134, 135, 136,
137, 138, 150, 171, 174
```

11 match modern KnC ignored list: `31, 32, 36, 54, 55, 56, 67, 72, 109, 128, 134`.

### Sample: opcode 1 (`sub_440230`)

```c
void Handler_01(CPacket* pkt) {
 char str[256];
 int type;
 pkt->ReadString(str);
 pkt->Read(&type, 4);
 ShowMessage(str, type); // sub_433AF0

 if (strstr(str, "MSG_SERVER_NOT_READY")) {
 byte_66DE60 = 0;
 sub_41A910(1, 1133903872); // reset to login
 }
 if (strstr(str, "MSG_DB_ACCESS_FAIL")) vtable[2](this, 0);
 if (strstr(str, "MSG_REINPUT_IDPASS")) vtable[2](this, 0);
 if (strstr(str, "MSG_INVALID_ID")) vtable[2](this, 0);
}
```

Same `MSG_*` tokens as modern protocol.

