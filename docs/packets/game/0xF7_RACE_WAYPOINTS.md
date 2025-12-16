# 0xF7 - RACE_WAYPOINTS

**CMD**: `0xF7` (247 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47DCA0`  
**Handler Ghidra**: `FUN_0047dca0`

## Description

Sends race waypoints/nodes data. Contains a count followed by 28-byte waypoint entries.

## Payload Structure

| Offset | Type   | Size   | Description           |
|--------|--------|--------|-----------------------|
| 0x00   | int32  | 4      | Entry count           |
| 0x04+  | entries| 28 × n | Waypoint entries      |

### Per-Entry Structure (28 bytes / 0x1C)

| Offset | Type  | Size | Description           |
|--------|-------|------|-----------------------|
| 0x00   | int32 | 4    | Waypoint ID           |
| 0x04   | float | 4    | X position            |
| 0x08   | float | 4    | Y position            |
| 0x0C   | float | 4    | Z position            |
| 0x10   | int32 | 4    | Flags / Type          |
| 0x14   | int32 | 4    | Next waypoint         |
| 0x18   | int32 | 4    | Extra data            |

**Total Size**: 4 + (28 × entryCount) bytes

## C Structure

```c
struct WaypointEntry {
    int32_t waypointId;         // +0x00
    float x;                    // +0x04
    float y;                    // +0x08
    float z;                    // +0x0C
    int32_t flags;              // +0x10
    int32_t nextWaypoint;       // +0x14
    int32_t extraData;          // +0x18
};

struct RaceWaypointsPacket {
    int32_t count;              // +0x00 - Number of waypoints
    WaypointEntry entries[];    // +0x04 - Array of waypoints
};
```

## Handler Logic (IDA)

```c
// sub_47DCA0
int __stdcall sub_47DCA0(int a1)
{
    int count;
    
    sub_44E910(a1, &count, 4);  // Read count
    
    int v4 = 171160 * dword_C7086C;  // Player slot offset
    
    for (int i = 0; i < count; ++i) {
        // Read 28 bytes into waypoint array
        sub_44E910(a1, (char*)&unk_1B1C7E4 + 28 * dword_1BC08EC[v4] + v4 * 4, 0x1C);
        ++dword_1BC08EC[v4];  // Increment waypoint counter
    }
    
    return count;
}
```

## Cross-Validation

| Source | Function       | Payload Read     |
|--------|----------------|------------------|
| IDA    | sub_47DCA0     | 4 + 28*n bytes   |
| Ghidra | FUN_0047dca0   | 4 + 28*n bytes   |

**Status**: ✅ CERTIFIED

