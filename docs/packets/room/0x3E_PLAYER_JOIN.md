# 0x3E PLAYER_JOIN (Server → Client)

**Status:** ✅ CERTIFIÉ (IDA + Ghidra)

**Handler IDA:** `sub_479D60` @ line 220591
**Handler Ghidra:** `FUN_00479d60` @ line 85118

## Purpose

Notification that a new player has joined the room. Contains complete player data.

## Payload Structure (Variable, ~180+ bytes)

```c
struct PlayerJoin {
    int32_t  playerId;         // +0x00 - Player unique ID
    wchar_t  playerName[35];   // +0x04 - Player name (70 bytes, null-padded)
    int32_t  unknown1;         // +var  - Unknown
    int32_t  unknown2;         // +var  - Unknown
    uint8_t  vehicleData[44];  // +var  - VehicleData (0x2C)
    uint8_t  itemData[56];     // +var  - ItemData (0x38)
    int32_t  unknown3;         // +var  - Unknown
    uint8_t  extraData[60];    // +var  - Extra data (0x3C)
};
```

## Field Details

| Order | Type | Size | Description |
|-------|------|------|-------------|
| 1 | int32 | 4 | Player ID |
| 2 | wstring | var | Player name (wchar_t[35]) |
| 3 | int32 | 4 | Unknown 1 |
| 4 | int32 | 4 | Unknown 2 |
| 5 | VehicleData | 44 | Equipped vehicle |
| 6 | ItemData | 56 | Equipped item |
| 7 | int32 | 4 | Unknown 3 |
| 8 | bytes | 60 | Extra data |

## Handler Code (IDA)

```c
void __thiscall sub_479D60(void *this, const wchar_t **a2)
{
  int playerId, unknown1, unknown2, unknown3;
  wchar_t playerName[35];
  int vehicleData[11];   // 44 bytes
  int itemData[14];      // 56 bytes (first element used as item type)
  int extraData[15];     // 60 bytes

  sub_44E910(a2, &playerId, 4);
  sub_44EB60(a2, playerName);
  sub_44E910(a2, &unknown1, 4);
  sub_44E910(a2, &unknown2, 4);
  sub_44E910(a2, vehicleData, 0x2C);
  sub_44E910(a2, itemData, 0x38);
  sub_44E910(a2, &unknown3, 4);
  sub_44E910(a2, extraData, 0x3C);
  
  // ... process player join
}
```

## Server Implementation

```cpp
void sendPlayerJoin(Session::Ptr session, const Player& player) {
    Packet pkt(0x3E);
    
    pkt.writeInt32(player.id);
    pkt.writeWString(player.name);  // Will be null-padded
    pkt.writeInt32(player.unknown1);
    pkt.writeInt32(player.unknown2);
    pkt.writeBytes(reinterpret_cast<const uint8_t*>(&player.vehicle), 44);
    pkt.writeBytes(reinterpret_cast<const uint8_t*>(&player.item), 56);
    pkt.writeInt32(player.unknown3);
    pkt.writeBytes(player.extraData, 60);
    
    session->send(pkt);
}
```

## Notes

- Sent to all players in room when someone joins
- Vehicle/Item data at offsets +1 used for lookups
- Extra 60 bytes purpose unknown
