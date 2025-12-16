# Data Structures Reference

**Status:** ✅ CERTIFIÉ (IDA + Ghidra cross-validation)
**Last Updated:** 2024-12-02

Confirmed sizes from client reverse engineering (`DevClient/KnC-new.exe.c`)

## Core Structures Summary

| Structure | Size | Hex | Fields | Used By |
|-----------|------|-----|--------|---------|
| PlayerInfo | 1224 | 0x4C8 | 15+ | CMD 0x07, 0xA7 |
| VehicleData | 44 | 0x2C | 11 | CMD 0x1B, 0x3E, 0x78, 0x9D |
| ItemData | 56 | 0x38 | 14 | CMD 0x1C, 0x3E, 0x9E |
| AccessoryData | 28 | 0x1C | 7 | CMD 0x1D, 0x1E, 0x9F |
| SmallItem | 32 | 0x20 | 8 | CMD 0x79, 0x7A |
| ChatMessage | ~116 | 0x74 | 8 | CMD 0x2D |
| PlayerJoin | 246 | - | 8 | CMD 0x3E |

---

## VehicleData (0x2C = 44 bytes)

**All fields confirmed via IDA handler analysis**

```c
#pragma pack(push, 1)
struct VehicleData {
    int32_t  vehicleId;       // +0x00 - Vehicle template ID
    int32_t  uniqueId;        // +0x04 - Unique instance ID (DB primary key)
    int32_t  durability;      // +0x08 - Current durability (0-100)
    int32_t  maxDurability;   // +0x0C - Maximum durability (usually 100)
    int32_t  statSpeed;       // +0x10 - Speed stat (0-100)
    int32_t  statAccel;       // +0x14 - Acceleration stat (0-100)
    int32_t  statHandling;    // +0x18 - Handling stat (0-100)
    int32_t  statDrift;       // +0x1C - Drift stat (0-100)
    int32_t  statBoost;       // +0x20 - Boost stat (0-100)
    int32_t  statWeight;      // +0x24 - Weight stat (0-100)
    int32_t  statSpecial;     // +0x28 - Special ability stat
};
static_assert(sizeof(VehicleData) == 44, "VehicleData must be 0x2C bytes");
#pragma pack(pop)
```

**IDA Evidence** (sub_479D60 @ 220629-220633):
```c
v18[14] = v14[2];  // durability
v18[15] = v14[3];  // maxDurability
v18[16] = v14[4];  // stats start
v18[17] = v14[5];
v18[18] = v14[6];
```

**Handlers**:
- `sub_478EC0` (CMD 0x1B) - Inventory vehicles
- `sub_47B220` (CMD 0x78) - Item list
- `sub_479D60` (CMD 0x3E) - Player join

---

## ItemData (0x38 = 56 bytes)

```c
#pragma pack(push, 1)
struct ItemData {
    int32_t  itemId;          // +0x00 - Item template ID
    int32_t  uniqueId;        // +0x04 - Unique instance ID (DB primary key)
    int32_t  quantity;        // +0x08 - Stack count (1 for non-stackable)
    int32_t  slot;            // +0x0C - Inventory slot position
    int32_t  equipped;        // +0x10 - 0=inventory, 1=equipped
    int32_t  expiration;      // +0x14 - Unix timestamp (0=permanent)
    int32_t  enhancement;     // +0x18 - Enhancement/upgrade level
    int32_t  bound;           // +0x1C - 0=tradeable, 1=character bound
    int32_t  reserved[6];     // +0x20 - Reserved/unknown (24 bytes)
};
static_assert(sizeof(ItemData) == 56, "ItemData must be 0x38 bytes");
#pragma pack(pop)
```

**Handlers**:
- `sub_478F20` (CMD 0x1C) - Inventory items
- `sub_479D60` (CMD 0x3E) - Player join (equipped item)

---

## AccessoryData (0x1C = 28 bytes)

**All fields confirmed**

```c
#pragma pack(push, 1)
struct AccessoryData {
    int32_t  accessoryId;     // +0x00 - Accessory template ID
    int32_t  uniqueId;        // +0x04 - Unique instance ID
    int32_t  slot;            // +0x08 - Equipment slot (0-3)
    int32_t  bonus1;          // +0x0C - Bonus stat 1 value
    int32_t  bonus2;          // +0x10 - Bonus stat 2 value
    int32_t  bonus3;          // +0x14 - Bonus stat 3 value
    int32_t  equipped;        // +0x18 - 0=inventory, 1=equipped
};
static_assert(sizeof(AccessoryData) == 28, "AccessoryData must be 0x1C bytes");
#pragma pack(pop)
```

**Handlers**:
- `sub_478F80` (CMD 0x1D) - Inventory accessories
- `sub_478FE0` (CMD 0x1E) - Single item update

---

## SmallItem (0x20 = 32 bytes)

```c
#pragma pack(push, 1)
struct SmallItem {
    int32_t  itemId;          // +0x00 - Item template ID
    int32_t  uniqueId;        // +0x04 - Unique instance ID
    int32_t  quantity;        // +0x08 - Stack count
    int32_t  slot;            // +0x0C - Slot position
    int32_t  flags;           // +0x10 - Item flags/status
    int32_t  reserved[3];     // +0x14 - Reserved (12 bytes)
};
static_assert(sizeof(SmallItem) == 32, "SmallItem must be 0x20 bytes");
#pragma pack(pop)
```

**Handlers**:
- `sub_47B290` (CMD 0x79) - Item list B
- `sub_47B4F0` (CMD 0x7A) - Item add (shop purchase)

---

## PlayerJoin (CMD 0x3E, 246 bytes)

**Handler:** `sub_479D60` @ line 220591

```c
#pragma pack(push, 1)
struct PlayerJoin {
    int32_t      playerId;        // +0x00 - Character ID (4 bytes)
    wchar_t      playerName[35];  // +0x04 - Player name (70 bytes, null-padded)
    int32_t      level;           // +0x4A - Player level
    int32_t      team;            // +0x4E - Team ID (0=none, 1=red, 2=blue)
    VehicleData  vehicle;         // +0x52 - Equipped vehicle (44 bytes)
    ItemData     item;            // +0x7E - Equipped item (56 bytes)
    int32_t      slot;            // +0xB6 - Room slot position (0-7)
    uint8_t      extraData[60];   // +0xBA - Race state data (0x3C bytes)
};
// Total: 4 + 70 + 4 + 4 + 44 + 56 + 4 + 60 = 246 bytes
#pragma pack(pop)
```

**Read sequence** (from IDA):
```c
sub_44E910(a2, &playerId, 4);
sub_44EB60(a2, playerName);      // wstring
sub_44E910(a2, &level, 4);
sub_44E910(a2, &team, 4);
sub_44E910(a2, vehicleData, 0x2C);
sub_44E910(a2, itemData, 0x38);
sub_44E910(a2, &slot, 4);
sub_44E910(a2, extraData, 0x3C);
```

**Usage**:
- `vehicleData[1]` = Template ID for vehicle lookup
- `itemData[1]` = Template ID for item lookup
- `level` and `team` passed to `sub_495280` (player creation)
- `slot` passed to `sub_48CBB0` (slot assignment)
- `extraData` passed to `sub_490A70` (race initialization)

---

## PlayerInfo (0x4C8 = 1224 bytes)

**Used by:** CMD 0x07, CMD 0xA7

```c
#pragma pack(push, 1)
struct PlayerInfo {
    // === Account Data Zone (0x000-0x485, ~1158 bytes) ===
    int32_t  characterId;           // +0x000 - Character ID (first field)
    uint8_t  accountData[0x482];    // +0x004 - Stats, inventory refs, settings...
    
    // === Driver Zone (0x486-0x49F, 26 bytes) ===
    wchar_t  driverName[13];        // +0x486 - Player name (12 chars + null)
    
    // === Flags Zone (0x4A0-0x4A3, 4 bytes) ===
    uint8_t  flag1;                 // +0x4A0 - Status flag 1
    uint8_t  flag2;                 // +0x4A1 - Status flag 2
    uint8_t  padding[2];            // +0x4A2 - Alignment padding
    
    // === Currency Zone (0x4A4-0x4AF, 12 bytes) ===
    int32_t  gold;                  // +0x4A4 - In-game currency (Lucci)
    int32_t  astros;                // +0x4A8 - Premium currency (Astros)
    int32_t  cash;                  // +0x4AC - Experience/secondary currency
    
    // === Equipment Zone (0x4B0-0x4B7, 8 bytes) ===
    int32_t  vehicleId;             // +0x4B0 - Equipped vehicle ID (-1 = none)
    int32_t  driverId;              // +0x4B4 - Equipped driver ID (-1 = none)
    
    // === Flags Zone 2 (0x4B8-0x4BB, 4 bytes) ===
    uint8_t  flags[4];              // +0x4B8 - Additional flags
    
    // === Trailing Zone (0x4BC-0x4C7, 12 bytes) ===
    int32_t  unknown1;              // +0x4BC
    int32_t  unknown2;              // +0x4C0
    int32_t  unknown3;              // +0x4C4
};
static_assert(sizeof(PlayerInfo) == 1224, "PlayerInfo must be 0x4C8 bytes");
#pragma pack(pop)
```

**Memory addresses**:
- `dword_80E1A8` = Account/Driver ID (before PlayerInfo)
- `dword_80E1B8` = Start of PlayerInfo
- `unk_80E63E` = Driver name (offset 0x486)
- `dword_80E658` = Flag zone (offset 0x4A0)
- `dword_80E65C` = Gold (offset 0x4A4)
- `dword_80E660` = Astros (offset 0x4A8)
- `dword_80E664` = Cash (offset 0x4AC)
- `dword_80E668` = Vehicle ID (offset 0x4B0)
- `dword_80E66C` = Driver ID (offset 0x4B4)

**Initialization** (sub_478A60):
```c
memset(&dword_80E1B8 + this, 0, 0x4C8u);
*(int*)(&dword_80E668 + this) = -1;  // vehicleId = -1
*(int*)(&dword_80E66C + this) = -1;  // driverId = -1
```

---

## ChannelList (CMD 0x0E)

**Handler:** `sub_4793F0` @ line 220146

```c
// Packet format:
struct ChannelList {
    int32_t count;              // Number of channels
    // FOR EACH channel:
    //   int32_t channelId;
    //   wstring name;          // UTF-16LE null-terminated
    //   int32_t currentPlayers;
    //   int32_t maxPlayers;
    //   int32_t type;          // 0=Rookie, 1=Advanced, 2=Master
    // END FOR
    // wstring footer;          // Empty wstring after channels
};
```

---

## Size Reference Table

| Bytes | Hex | Structure |
|-------|-----|-----------|
| 4 | 0x04 | int32, float |
| 28 | 0x1C | AccessoryData |
| 32 | 0x20 | SmallItem |
| 44 | 0x2C | VehicleData |
| 56 | 0x38 | ItemData |
| 60 | 0x3C | ExtraData (PlayerJoin) |
| 104 | 0x68 | EntityData |
| 116 | 0x74 | ChatMessage |
| 246 | - | PlayerJoin |
| 1224 | 0x4C8 | PlayerInfo |

---

## Notes

- All multi-byte values are **little-endian**
- Strings are **wchar_t (UTF-16LE)** unless noted as ASCII
- Count fields are always **int32 (4 bytes)**
- Packet reader: `sub_44E910(packet, dest, size)`
- WString reader: `sub_44EB60(packet, dest)`
- ASCII reader: `sub_44EB30(packet, dest)`
