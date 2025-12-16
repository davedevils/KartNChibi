# 0x46 LARGE_GAME_STATE (Server → Client)

**Packet ID:** 0x46 (70 decimal)  
**Handler:** `sub_47A760` / `FUN_0047a760`  
**Status:** ✅ CERTIFIÉ IDA+Ghidra

## Purpose

Sends large game state with multiple player entries.

## Payload Structure

```c
struct LargeGameState {
    int32 localPlayerId;    // Current player's ID
    int32 entryCount;       // Number of player entries
    PlayerEntry entries[entryCount];
};

struct PlayerEntry {
    int32   field1;          // 4 bytes
    int32   field2;          // 4 bytes
    int32   playerId;        // 4 bytes
    wchar_t nickname[13];    // wstring
    byte    flag1;           // 1 byte
    int32   field3;          // 4 bytes
    int32   field4;          // 4 bytes
    int32   field5;          // 4 bytes
    int32   field6;          // 4 bytes
    int32   field7;          // 4 bytes
    byte    flag2;           // 1 byte
    int32   field8;          // 4 bytes
    int32   field9;          // 4 bytes
    int32   field10;         // 4 bytes
};
```

## Size

**Variable:** 8 + (entryCount × ~55 bytes per entry)

## Fields - Header

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Local player ID (stored at 80E1B4) |
| 0x04 | int32 | 4 | Number of entries |

## Fields - Per Entry (~55 bytes each)

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Field 1 (v9) |
| 0x04 | int32 | 4 | Field 2 (v19) |
| 0x08 | int32 | 4 | Player ID (v10) |
| 0x0C | wstring | var | Nickname |
| +0x00 | byte | 1 | Flag 1 |
| +0x01 | int32 | 4 | Field 3 |
| +0x05 | int32 | 4 | Field 4 |
| +0x09 | int32 | 4 | Field 5 |
| +0x0D | int32 | 4 | Field 6 |
| +0x11 | int32 | 4 | Field 7 |
| +0x15 | byte | 1 | Flag 2 |
| +0x16 | int32 | 4 | Field 8 |
| +0x1A | int32 | 4 | Field 9 |
| +0x1E | int32 | 4 | Field 10 |

## Handler Logic

```c
// IDA: sub_47A760 / Ghidra: FUN_0047a760
clearPlayerList();
read(&localPlayerId, 4);
read(&entryCount, 4);

for (i = 0; i < entryCount; i++) {
    read(&field1, 4);
    read(&field2, 4);
    read(&playerId, 4);
    read_wstring(&nickname);
    read(&flag1, 1);
    read(&field3, 4);
    read(&field4, 4);
    read(&field5, 4);
    read(&field6, 4);
    read(&field7, 4);
    read(&flag2, 1);
    read(&field8, 4);
    read(&field9, 4);
    read(&field10, 4);
    
    addPlayerEntry(...);
}
```

## Server Implementation

```cpp
void sendLargeGameState(Session::Ptr session, int localId, 
                        const std::vector<PlayerState>& players) {
    Packet pkt(0x46);
    pkt.writeInt32(localId);
    pkt.writeInt32(players.size());
    
    for (auto& p : players) {
        pkt.writeInt32(p.field1);
        pkt.writeInt32(p.field2);
        pkt.writeInt32(p.playerId);
        pkt.writeWString(p.nickname);
        pkt.writeUInt8(p.flag1);
        pkt.writeInt32(p.field3);
        pkt.writeInt32(p.field4);
        pkt.writeInt32(p.field5);
        pkt.writeInt32(p.field6);
        pkt.writeInt32(p.field7);
        pkt.writeUInt8(p.flag2);
        pkt.writeInt32(p.field8);
        pkt.writeInt32(p.field9);
        pkt.writeInt32(p.field10);
    }
    session->send(pkt);
}
```

## Notes

- Complex packet with multiple player entries
- Used for full game state synchronization
- Entry size is variable due to wstring nickname
