# 0xA7 SESSION_CONFIRM / PLAYER_DATA (Server → Client)

**Packet ID:** 0xA7 (167 decimal)
**Handler:** `sub_479080` @ 0x00479080

## Purpose

Sends player data to client - **same format as 0x07 response!**
Contains Driver ID and full 1224-byte PlayerInfo structure.

This packet contains **FULL PLAYER DATA**!

## Payload Structure

```c
struct SessionConfirm {
    int32    driverId;       // 4 bytes - stored at dword_80E1A8 (Driver ID!)
    uint8_t  playerInfo[1224]; // 0x4C8 bytes - stored at dword_80E1B8
};
```

## Size

**1228 bytes** (4 + 1224)

## Handler Decompilation (IDA)

```c
int __thiscall sub_479080(void *this, int a2)
{
    int result;
    
    // Read Driver ID (4 bytes) -> dword_80E1A8
    sub_44E910(a2, (char *)&dword_80E1A8 + (_DWORD)this, (const char *)4);
    
    // Read PlayerInfo (0x4C8 = 1224 bytes) -> dword_80E1B8
    result = sub_44E910(a2, (char *)&dword_80E1B8 + (_DWORD)this, (const char *)0x4C8);
    
    // Set flag indicating data received
    byte_8CCDC0[(_DWORD)this] = 1;
    
    return result;
}
```

## Important Variables

| Variable | Offset | Size | Description |
|----------|--------|------|-------------|
| `dword_80E1A8` | +0x80E1A8 | 4 | **Driver ID** (same as in 0x07!) |
| `dword_80E1B8` | +0x80E1B8 | 1224 | **PlayerInfo structure** |
| `byte_8CCDC0` | +0x8CCDC0 | 1 | Data received flag |

## Relationship to 0x07

Both 0x07 and 0xA7 use the **same data structure**:

| Packet | Direction | Content |
|--------|-----------|---------|
| 0x07 | S→C (Login) | AccountID + PlayerInfo (1228 bytes) |
| 0xA7 | S→C (Game) | DriverID + PlayerInfo (1228 bytes) |

The offsets are the same:
- `dword_80E1A8` = Driver ID (both packets)
- `dword_80E1B8` = PlayerInfo start (both packets)

## Server Implementation

```cpp
void sendSessionConfirm(Session::Ptr session, int32_t driverId, 
                        const PlayerInfo& playerInfo) {
    Packet pkt(0xA7);
    
    // Write Driver ID
    pkt.writeInt32(driverId);
    
    // Write PlayerInfo (1224 bytes)
    pkt.writeBytes(&playerInfo, 1224);
    
    session->send(pkt);
}
```

## Example Packet

```
Payload (1228 bytes):
[XX XX XX XX]         // Driver ID (4 bytes)
[... 1224 bytes ...]  // PlayerInfo structure
```

## Usage

- **GameServer**: Send after player connects with valid session
- Contains current character data
- Sets `byte_8CCDC0 = 1` to indicate data is loaded

## Notes

- This is essentially the **same as 0x07** but sent by GameServer
- Driver ID of -1 (0xFFFFFFFF) = no character
- Driver ID of 0 = also no character (or invalid)
- The flag `byte_8CCDC0` is checked elsewhere for data validity
