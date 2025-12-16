# 0x21 ROOM_PLAYER_INFO (Server → Client)

**Status:** ✅ CERTIFIÉ (IDA + Ghidra)

**Handler IDA:** `sub_4797C0` @ line 220351
**Handler Ghidra:** `FUN_004797c0` @ line 84857

## Purpose

Sends detailed player information when joining a room. Contains player data, vehicle, item, and additional info.

## Payload Structure (Variable, ~200+ bytes)

```c
struct RoomPlayerInfo {
    int32_t  playerId;         // +0x00 - Player ID
    int32_t  unknown1;         // +0x04 - Unknown
    int32_t  unknown2;         // +0x08 - Unknown
    wchar_t  playerName[];     // +0x0C - Player name (wstring)
    uint8_t  flag1;            // +var  - Flag byte
    uint8_t  flag2;            // +var  - Flag byte
    uint8_t  flag3;            // +var  - Flag byte
    int32_t  unknown3;         // +var  - Unknown
    uint8_t  vehicleData[44];  // +var  - VehicleData (0x2C)
    uint8_t  itemData[56];     // +var  - ItemData (0x38)
    int32_t  unknown4;         // +var  - Unknown
    int32_t  unknown5;         // +var  - Unknown
    uint8_t  extraData[60];    // +var  - Extra data (0x3C)
};
```

## Field Details

| Order | Type | Size | Description |
|-------|------|------|-------------|
| 1 | int32 | 4 | Player ID |
| 2 | int32 | 4 | Unknown 1 |
| 3 | int32 | 4 | Unknown 2 |
| 4 | wstring | var | Player name (UTF-16LE null-term) |
| 5 | uint8 | 1 | Flag 1 |
| 6 | uint8 | 1 | Flag 2 |
| 7 | uint8 | 1 | Flag 3 |
| 8 | int32 | 4 | Unknown 3 |
| 9 | VehicleData | 44 | Equipped vehicle |
| 10 | ItemData | 56 | Equipped item |
| 11 | int32 | 4 | Unknown 4 |
| 12 | int32 | 4 | Unknown 5 |
| 13 | bytes | 60 | Extra data |

## Handler Code (IDA)

```c
char __stdcall sub_4797C0(const wchar_t **a1)
{
  int playerId, unknown1, unknown2, unknown3, unknown4, unknown5;
  wchar_t playerName[13];
  char flag1, flag2, flag3;
  int vehicleData[11];   // 44 bytes
  int itemData[14];      // 56 bytes
  int extraData[15];     // 60 bytes

  sub_44E910(a1, &playerId, 4);
  sub_44E910(a1, &unknown1, 4);
  sub_44E910(a1, &unknown2, 4);
  sub_44EB60(a1, playerName);
  sub_44E910(a1, &flag1, 1);
  sub_44E910(a1, &flag2, 1);
  sub_44E910(a1, &flag3, 1);
  sub_44E910(a1, &unknown3, 4);
  sub_44E910(a1, vehicleData, 0x2C);
  sub_44E910(a1, itemData, 0x38);
  sub_44E910(a1, &unknown4, 4);
  sub_44E910(a1, &unknown5, 4);
  sub_44E910(a1, extraData, 0x3C);
  
  return sub_40D650(dword_B9ADD0, playerId, unknown1, unknown2, playerName, 
                   flag1, flag2, flag3, unknown3, vehicleData, itemData, 
                   unknown5, extraData, unknown4);
}
```

## Server Implementation

```cpp
void sendRoomPlayerInfo(Session::Ptr session, const RoomPlayer& player) {
    Packet pkt(0x21);
    
    pkt.writeInt32(player.id);
    pkt.writeInt32(player.unknown1);
    pkt.writeInt32(player.unknown2);
    pkt.writeWString(player.name);
    pkt.writeUInt8(player.flag1);
    pkt.writeUInt8(player.flag2);
    pkt.writeUInt8(player.flag3);
    pkt.writeInt32(player.unknown3);
    pkt.writeBytes(player.vehicleData, 44);
    pkt.writeBytes(player.itemData, 56);
    pkt.writeInt32(player.unknown4);
    pkt.writeInt32(player.unknown5);
    pkt.writeBytes(player.extraData, 60);
    
    session->send(pkt);
}
```

## Notes

- Previously mislabeled as "Room Full" - actually contains player info
- Complex structure with vehicle and item data embedded

