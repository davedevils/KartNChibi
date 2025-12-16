# 0x30 - ROOM_STATE

**CMD**: `0x30` (48 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_479760`  
**Handler Ghidra**: `FUN_00479760`

## Description

Updates the ready state of a player in a room. When a player toggles their ready status, this packet is sent to update all clients.

## Payload Structure

| Offset | Type   | Size | Description           |
|--------|--------|------|-----------------------|
| 0x00   | int32  | 4    | Player ID             |

**Total Size**: 4 bytes

## C Structure

```c
struct RoomStatePacket {
    int32_t playerId;           // +0x00 - ID of the player whose state changed
};
```

## Handler Logic (IDA)

```c
// sub_479760
_DWORD *__stdcall sub_479760(int a1)
{
    sub_44E910(a1, &dword_BCE220, 4);  // Read playerId
    
    // Iterates through player array
    // If playerId matches and state == 1, sets state to 0
    // If playerId matches current player, clears ready flag
}
```

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_479760     | 4 bytes      |
| Ghidra | FUN_00479760   | 4 bytes      |

**Status**: ✅ CERTIFIED

