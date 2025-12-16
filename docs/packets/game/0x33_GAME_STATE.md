# 0x33 - GAME_STATE

**CMD**: `0x33` (51 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_4799B0`  
**Handler Ghidra**: `FUN_004799b0`

## Description

Updates game state for a player during a race. When `action` is 2, sets a global flag and triggers a specific game action.

## Payload Structure

| Offset | Type   | Size | Description           |
|--------|--------|------|-----------------------|
| 0x00   | int32  | 4    | Player ID             |
| 0x04   | int32  | 4    | Action / State code   |

**Total Size**: 8 bytes

## C Structure

```c
struct GameStatePacket {
    int32_t playerId;           // +0x00 - Target player ID
    int32_t action;             // +0x04 - State/action code
};
```

## Action Values

| Value | Effect                                    |
|-------|-------------------------------------------|
| 2     | Sets `dword_BCE228 = 1`, calls `sub_40A040` |
| Other | Calls `sub_409FB0(playerId, action)`      |

## Handler Logic (IDA)

```c
// sub_4799B0
char __stdcall sub_4799B0(int a1)
{
    int v3;  // playerId
    int a1;  // action (reused)
    
    sub_44E910(a1, &v3, 4);      // Read playerId
    sub_44E910(a1, &a1, 4);      // Read action
    
    if (a1 != 2)
        return sub_409FB0(dword_B9ADD0, v3, a1);
    
    dword_BCE228 = 1;
    return sub_40A040(dword_B9ADD0);
}
```

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_4799B0     | 8 bytes      |
| Ghidra | FUN_004799b0   | 8 bytes      |

**Status**: ✅ CERTIFIED
