# 0x54 SERVER_REDIRECT (Server → Client)

**Handler:** `sub_47AA00`

## Purpose

**Critical packet** - Redirects client to another server (e.g., from LoginServer to GameServer).

## Payload Structure

```c
struct ServerRedirect {
    int32 unknown;
    char  serverIP[256];     // Null-terminated ASCII
    int32 port;
};
```

## Size

**~264 bytes** (variable due to string)

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Unknown (possibly server type) |
| 0x04 | string | 256 | Server IP address |
| var | int32 | 4 | Server port |

## Flow

1. Server sends this packet with new IP/port
2. Client calls `sub_4774C0` to connect
3. On success: Sleep 1000ms, then send state 8
4. On failure: Display `MSG_CONNECT_FAIL`

## Notes

- Essential for multi-server architecture
- Used to transfer players between LoginServer and GameServer
- May also be used for channel switching

