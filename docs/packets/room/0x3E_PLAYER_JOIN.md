# CMD 0x3E (62) - Player Join Room

**Direction:** Server → Client  
**Handler:** `sub_479D60`

## Structure 

```c
struct PlayerJoin {
    int32 unknown1;                 // +0x00
    wchar_t playerName[35];         // +0x04 (70 bytes)
    int32 unknown2;                 // +0x48
    int32 unknown3;                 // +0x4C
    VehicleData vehicle;            // +0x50 (0x2C = 44 bytes)
    ItemData equippedItem;          // +0x7C (0x38 = 56 bytes)
    // Total: ~180 bytes minimum
};

struct VehicleData {                // 0x2C = 44 bytes
    int32 vehicleId;
    int32 ownerId;
    int32 durability;
    int32 maxDurability;
    int32 stats[7];
};

struct ItemData {                   // 0x38 = 56 bytes
    int32 itemId;
    int32 ownerId;
    int32 quantity;
    int32 unknown[11];
};
```

## Handler Code Pattern

```c
sub_44E910((int)a2, &v8, (const char *)4);           // Read int32
v3 = sub_44EB60(a2, String);                         // Read wstring
sub_44E910((int)v3, &v11, (const char *)4);          // Read int32
sub_44E910((int)v3, &v10, (const char *)4);          // Read int32
sub_44E910((int)a2, v14, (const char *)0x2C);        // Read 44 bytes (vehicle)
sub_44E910((int)a2, v15, (const char *)0x38);        // Read 56 bytes (item)
```

## Notes

- Player name is wchar_t (UTF-16LE), max 35 chars
- Vehicle and item data are full structures
- Broadcast to all players in room when someone joins
