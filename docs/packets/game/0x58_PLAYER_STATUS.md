# 0x58 - PLAYER_STATUS

**CMD**: `0x58` (88 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47ABE0`  
**Handler Ghidra**: `FUN_0047abe0`

## Description

Updates a player's status in the race/game. Uses a lookup function to find the player and update their status byte.

## Payload Structure

| Offset | Type   | Size | Description           |
|--------|--------|------|-----------------------|
| 0x00   | int32  | 4    | Player ID             |
| 0x04   | byte   | 1    | Status value          |

**Total Size**: 5 bytes

## C Structure

```c
struct PlayerStatusPacket {
    int32_t playerId;           // +0x00 - Target player ID
    uint8_t status;             // +0x04 - New status value
};
```

## Handler Logic (IDA)

```c
// sub_47ABE0
char __stdcall sub_47ABE0(int a1)
{
    int playerId;
    uint8_t status;
    
    sub_44E910(a1, &playerId, 4);   // Read player ID
    sub_44E910(a1, &status, 1);     // Read status byte
    
    // Find player in dword_1AF2BB0
    int idx = sub_48B430(dword_1AF2BB0, playerId);
    if (idx >= 0)
        return sub_48B460(dword_1AF2BB0, idx, status);
    
    return idx;
}
```

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_47ABE0     | 5 bytes      |
| Ghidra | FUN_0047abe0   | 5 bytes      |

**Status**: ✅ CERTIFIED
