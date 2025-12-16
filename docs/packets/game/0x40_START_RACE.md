# 0x40 - START_RACE

**CMD**: `0x40` (64 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47FD30`  
**Handler Ghidra**: `FUN_0047fd30`

## Description

Initiates a race by sending initial positions and orientations for all players. This is a variable-length packet containing data for each player in the race.

## Payload Structure

| Offset | Type   | Size | Description           |
|--------|--------|------|-----------------------|
| 0x00   | byte   | 1    | Player count          |
| 0x01+  | ...    | 25*n | Player data array     |

**Total Size**: 1 + (25 × playerCount) bytes

### Per-Player Data (25 bytes each)

| Offset | Type   | Size | Description              |
|--------|--------|------|--------------------------|
| 0x00   | int32  | 4    | Player ID                |
| 0x04   | int16  | 2    | Unknown (v14/local_50)   |
| 0x06   | int64  | 8    | Position (v17/local_44)  |
| 0x0E   | int64  | 8    | Rotation (v18/local_3c)  |
| 0x16   | byte   | 1    | Unknown (v13/local_51)   |
| 0x17   | int16  | 2    | Unknown (v15/local_4c)   |

**Per-Player Size**: 25 bytes

## C Structure

```c
struct PlayerRaceData {
    int32_t playerId;           // +0x00 - Player ID
    int16_t unknown1;           // +0x04 - Unknown
    int64_t position;           // +0x06 - Starting position (packed)
    int64_t rotation;           // +0x0E - Starting rotation (packed)
    uint8_t unknown2;           // +0x16 - Unknown flag
    int16_t unknown3;           // +0x17 - Unknown
};  // 25 bytes

struct StartRacePacket {
    uint8_t playerCount;        // +0x00 - Number of players
    PlayerRaceData players[];   // +0x01 - Variable length array
};
```

## Handler Logic (IDA)

```c
// sub_47FD30
char __stdcall sub_47FD30(int a1)
{
    int v1 = a1;
    char count;
    
    sub_44E910(a1, &count, 1);  // Read player count
    
    if (!sub_487230(dword_1ADF810) && count > 0) {
        for (int i = 0; i < count; i++) {
            int playerId;
            __int16 v14;
            __int64 position, rotation;
            char flag;
            __int16 v15;
            
            sub_44E910(v1, &playerId, 4);
            sub_44E910(v1, &v14, 2);
            sub_44E910(v1, &position, 8);
            sub_44E910(v1, &rotation, 8);
            sub_44E910(v1, &flag, 1);
            sub_44E910(v1, &v15, 2);
            
            int idx = sub_48DEA0(byte_1B19090, playerId);
            if (idx >= 0) {
                // Convert packed position/rotation to floats
                // Update player's race state
                // Initialize race for player
            }
        }
    }
    
    return ...;
}
```

## Notes

- The `int64` position and rotation values are packed 3D vectors
- They are converted to float arrays via `sub_44E7F0`
- Position: 3 × float (x, y, z)
- Rotation: 3 × float (rx, ry, rz)

## Cross-Validation

| Source | Function       | Payload Read       |
|--------|----------------|--------------------|
| IDA    | sub_47FD30     | 1 + 25*n bytes     |
| Ghidra | FUN_0047fd30   | 1 + 25*n bytes     |

**Status**: ✅ CERTIFIED

