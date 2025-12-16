# 0xA6 HEARTBEAT (Client → Server)

**Packet ID:** 0xA6 (166 decimal)
**Builder:** `sub_477610` @ 0x00477610

## Purpose

Client sends periodic heartbeat to keep connection alive.

## Payload Structure

```c
struct Heartbeat {
    int32 sessionData;    // From dword_80E07C, reset to 0 after send
};
```

## Size

**4 bytes**

## Builder Decompilation (IDA)

```c
void __thiscall sub_477610(_DWORD *this)
{
    // Get current timestamp
    v2 = sub_44ED50(&off_5E1B00);
    
    // Get last send time
    v3 = *(int *)((char *)&dword_8CCDB0 + (_DWORD)this);  // low
    v4 = *(int *)((char *)&dword_8CCDB4 + (_DWORD)this);  // high
    
    v5 = v2 - __PAIR64__(v4, v3);  // Time since last send
    
    // Only send if >= 1000ms (0x3E8) since last send
    if (v5 >= 0x3E8)  // Rate limit: 1 second
    {
        // Update last send time
        *(int *)((char *)&dword_8CCDB0 + (_DWORD)this) = v2;
        *(int *)((char *)&dword_8CCDB4 + (_DWORD)this) = HIDWORD(v2);
        
        // Build packet CMD=166 (0xA6)
        sub_44ECD0(v6, 166);
        v7 = 0;
        
        // Write sessionData (4 bytes from dword_80E07C)
        sub_44E9C0(v6, (char *)&dword_80E07C + (_DWORD)this, (const char *)4);
        
        // Send
        sub_476B80(this, v6);
        
        // Reset sessionData after send
        *(int *)((char *)&dword_80E07C + (_DWORD)this) = 0;
        
        // Cleanup
        v7 = -1;
        sub_44E8E0(v6);
    }
}
```

## Rate Limiting

- **Minimum interval:** 1000ms (0x3E8)
- Client checks timestamp before each send
- Won't spam - maximum 1 heartbeat per second

## Important Variables

| Variable | Offset | Description |
|----------|--------|-------------|
| `dword_80E07C` | +0x80E07C | Session data to send |
| `dword_8CCDB0` | +0x8CCDB0 | Last send time (low) |
| `dword_8CCDB4` | +0x8CCDB4 | Last send time (high) |

## Raw Packet Example

```
04 00 A6 00 00 00 00 00  E0 04 00 00
│  │  │               │  └─────────── sessionData (0x04E0 = 1248)
│  │  │               └───────────── flags (zeros)
│  │  └───────────────────────────── CMD = 0xA6 (166)
│  └──────────────────────────────── size = 4
└─────────────────────────────────── size (low byte)
```

## Server Handling

### LoginServer
```cpp
void handleHeartbeat(Session::Ptr session, Packet& pkt) {
    // Just ignore - no response needed!
    // DO NOT send 0x12 - it will send client to lobby!
    LOG_DEBUG("AUTH", "Heartbeat ignored");
}
```

### GameServer
```cpp
void handleHeartbeat(Session::Ptr session, Packet& pkt) {
    int32_t sessionData = pkt.readInt32();
    // May need to respond or update session timestamp
    session->lastHeartbeat = std::chrono::steady_clock::now();
}
```

## Notes

- Heartbeat is **NOT spam** - rate limited to 1/second by client
- `sessionData` is reset to 0 after each send
- Server should track last heartbeat for timeout detection
- **LoginServer should IGNORE heartbeats** (no response!)
- **GameServer may need to respond** (needs investigation)
