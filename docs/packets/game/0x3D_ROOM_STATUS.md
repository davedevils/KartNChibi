# 0x3D - ROOM_STATUS

**CMD**: `0x3D` (61 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47A6B0`  
**Handler Ghidra**: `FUN_0047a6b0`

## Description

Updates a player's status within a room. Used for ready state, loading state, or other room-related statuses.

## Payload Structure

| Offset | Type   | Size | Description           |
|--------|--------|------|-----------------------|
| 0x00   | int32  | 4    | Player ID             |
| 0x04   | int32  | 4    | Status value          |

**Total Size**: 8 bytes

## C Structure

```c
struct RoomStatusPacket {
    int32_t playerId;           // +0x00 - Target player ID
    int32_t status;             // +0x04 - New status value
};
```

## Handler Logic (IDA)

```c
// sub_47A6B0
char __stdcall sub_47A6B0(int a1)
{
    int v1 = a1;
    int v5;  // status
    
    sub_44E910(a1, &a1, 4);     // Read playerId
    sub_44E910(v1, &v5, 4);     // Read status
    
    int idx = sub_48DEA0(byte_1B19090, a1);  // Find player index
    if (idx >= 0) {
        sub_498F60(byte_1B19090, idx, v5);  // Update player status
        
        if (a1 == dword_1A20658)  // If local player
            sub_4B0CD0(byte_2EB89A0);
        
        sub_48B460(dword_1AF2BB0, dword_1B1C5B0[171160 * idx], 9);
        
        if (idx == dword_1B19744) {
            sub_43ED70(&off_5C8310, 9, -1, 1065353216);
        } else if (!v5) {
            sub_4A9AC0(&unk_2EB24F0, idx);
        }
    }
    return v2;
}
```

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_47A6B0     | 8 bytes      |
| Ghidra | FUN_0047a6b0   | 8 bytes      |

**Status**: ✅ CERTIFIED

