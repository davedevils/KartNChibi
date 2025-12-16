# 0x117 - PLAYER_REMOVE

**CMD**: `0x117` (279 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47E750`  
**Handler Ghidra**: `FUN_0047e750`

## Description

Removes a player from the chat/player list by ID.

## Payload Structure

| Offset | Type  | Size | Description           |
|--------|-------|------|-----------------------|
| 0x00   | int32 | 4    | Player ID             |

**Total Size**: 4 bytes

## C Structure

```c
struct PlayerRemovePacket {
    int32_t playerId;           // +0x00
};
```

## Handler Logic (IDA)

```c
// sub_47E750
char __stdcall sub_47E750(int a1)
{
    int playerId;
    
    sub_44E910(a1, &playerId, 4);
    return sub_40F5F0(dword_B9ADD0, playerId);
}
```

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_47E750     | 4 bytes      |
| Ghidra | FUN_0047e750   | 4 bytes      |

**Status**: ✅ CERTIFIED

