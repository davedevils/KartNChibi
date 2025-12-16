# 0x13 PLAYER_ROOM_DATA (Server → Client)

**Handler:** `sub_47FC20` / `FUN_0047fc20`  
**Status:** ✅ CERTIFIÉ IDA+Ghidra

## Purpose

Populates room/lobby player list data. Sets UI state to 9 (room view).

## Payload Structure

```c
struct PlayerRoomData {
    int32   roomId;           // 4 bytes - Room identifier
    wchar_t roomName[60];     // wstring (null-terminated UTF-16LE)
    int32   field_22C;        // 4 bytes
    int32   field_210;        // 4 bytes
    int32   field_224;        // 4 bytes
    int32   field_214;        // 4 bytes
    int32   field_218;        // 4 bytes
    int32   field_21C;        // 4 bytes
    int32   field_220;        // 4 bytes
    int32   field_244;        // 4 bytes
    int32   playerCount;      // 4 bytes - Number of players
    PlayerEntry players[playerCount];  // 48 bytes each
};

struct PlayerEntry {
    byte data[48];  // 0x30 bytes per player
};
```

## Size

**Variable:** 4 + wstring + 36 + 4 + (48 × playerCount)

Minimum: ~44 bytes (no players, short name)

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Room ID (stored at BCE1B0) |
| 0x04 | wstring | var | Room name (stored at BCE1B4) |
| +0x00 | int32 | 4 | Field BCE22C |
| +0x04 | int32 | 4 | Field BCE210 |
| +0x08 | int32 | 4 | Field BCE224 |
| +0x0C | int32 | 4 | Field BCE214 |
| +0x10 | int32 | 4 | Field BCE218 |
| +0x14 | int32 | 4 | Field BCE21C |
| +0x18 | int32 | 4 | Field BCE220 |
| +0x1C | int32 | 4 | Field BCE244 |
| +0x20 | int32 | 4 | Player count |
| +0x24 | array | 48×n | Player entries |

## Handler Logic

```c
// IDA: sub_47FC20 / Ghidra: FUN_0047fc20
if (dword_F727F4 != 2) {  // UI gate check
    read(&roomId, 4);
    read_wstring(&roomName);
    read(&field_22C, 4);
    read(&field_210, 4);
    read(&field_224, 4);
    read(&field_214, 4);
    read(&field_218, 4);
    read(&field_21C, 4);
    read(&field_220, 4);
    read(&field_244, 4);
    
    clearPlayerList();
    read(&playerCount, 4);
    
    for (i = 0; i < playerCount; i++) {
        read(playerEntry, 0x30);  // 48 bytes
        addPlayerToList(playerEntry);
    }
    
    byte_BCE248 = 0;
    setUIState(9);  // Show room view
}
```

## Important Variables

| Variable | Address | Description |
|----------|---------|-------------|
| dword_BCE1B0 | 0xBCE1B0 | Room ID |
| unk_BCE1B4 | 0xBCE1B4 | Room name buffer |
| dword_BCE850 | 0xBCE850 | Player list array |
| byte_BCE248 | 0xBCE248 | Flag (set to 0) |

## UI State

Sets `dword_B23288` to **9** (room/lobby view).

## Server Implementation

```cpp
void sendPlayerRoomData(Session::Ptr session, Room& room) {
    Packet pkt(0x13);
    
    pkt.writeInt32(room.id);
    pkt.writeWString(room.name);
    pkt.writeInt32(room.field22C);
    pkt.writeInt32(room.field210);
    pkt.writeInt32(room.field224);
    pkt.writeInt32(room.field214);
    pkt.writeInt32(room.field218);
    pkt.writeInt32(room.field21C);
    pkt.writeInt32(room.field220);
    pkt.writeInt32(room.field244);
    
    pkt.writeInt32(room.players.size());
    for (auto& player : room.players) {
        pkt.writeBytes(player.entryData, 48);
    }
    
    session->send(pkt);
}
```

## Notes

- **Critical**: Only processed if `dword_F727F4 != 2` (UI gate)
- Clears existing player list before adding new entries
- Each player entry is exactly 48 bytes (0x30)
- Triggers UI state change to room view (state 9)
