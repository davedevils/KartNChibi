# 0x54 - SERVER_REDIRECT

**CMD**: `0x54` (84 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47AA00`  
**Handler Ghidra**: `FUN_0047aa00`

## Description

Redirects the client to connect to a different server (typically from login server to game server). The client will disconnect from the current server and connect to the new IP/port specified.

## Payload Structure

| Offset | Type   | Size     | Description           |
|--------|--------|----------|-----------------------|
| 0x00   | int32  | 4        | Session/token (unknown) |
| 0x04   | string | variable | Server IP address (ASCII, null-terminated) |
| ?      | int32  | 4        | Server port           |

**Total Size**: Variable (8 + string length + 1)

## C Structure

```c
struct ServerRedirectPacket {
    int32_t sessionToken;       // +0x00 - Session/token to carry over
    char serverIP[];            // +0x04 - ASCII null-terminated IP address
    // After string:
    int32_t serverPort;         // Port number
};
```

## Handler Logic (IDA)

```c
// sub_47AA00
void __thiscall sub_47AA00(ULONG_PTR CompletionKey, const char **a2)
{
    int sessionToken;
    char ip[256];
    u_short port;
    
    sub_44E910(a2, &sessionToken, 4);     // Read session token
    sub_44EB30(a2, ip);                    // Read IP string
    sub_44E910(a2, &port, 4);             // Read port
    
    if (sub_4774C0(CompletionKey, ip, port)) {  // Connect to new server
        Sleep(1000);  // Wait 1 second
        sub_483FF0(CompletionKey, 8);     // Update UI state
    } else {
        byte_B23180 = 0;
        sub_43D7E0(flt_D6E188, 1, 1133903872);
        sub_4641E0(byte_F727E8, "MSG_CONNECT_FAIL", 2);
    }
}
```

## Flow

```
┌─────────────────────────────────────────┐
│ Client connected to Login Server        │
├─────────────────────────────────────────┤
│ Login Server sends 0x54                 │
│   - sessionToken                        │
│   - gameServerIP                        │
│   - gameServerPort                      │
├─────────────────────────────────────────┤
│ Client disconnects from Login Server    │
│ Client connects to Game Server          │
│ Client uses sessionToken for auth       │
└─────────────────────────────────────────┘
```

## Cross-Validation

| Source | Function       | Payload Read                 |
|--------|----------------|------------------------------|
| IDA    | sub_47AA00     | 4 + string + 4               |
| Ghidra | FUN_0047aa00   | 4 + string + 4               |

**Status**: ✅ CERTIFIED

