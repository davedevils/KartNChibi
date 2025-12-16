# 0x47 - PLAYER_ACTION

**CMD**: `0x47` (71 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47A110`  
**Handler Ghidra**: `FUN_0047a110`

## Description

Triggers various player actions based on an action type. Used for items, effects, and special abilities during gameplay.

## Payload Structure

| Offset | Type   | Size | Description              |
|--------|--------|------|--------------------------|
| 0x00   | int32  | 4    | Player ID                |
| 0x04   | int32  | 4    | Action type              |
| 0x08   | int32  | 4    | Parameter 1 (float cast) |
| 0x0C   | int32  | 4    | Parameter 2 (float cast) |
| 0x10   | int32  | 4    | Parameter 3 (float cast) |
| 0x14   | int32  | 4    | Parameter 4 (float cast) |

**Total Size**: 24 bytes

## C Structure

```c
struct PlayerActionPacket {
    int32_t playerId;           // +0x00 - Target player ID
    int32_t actionType;         // +0x04 - Type of action
    float param1;               // +0x08 - Parameter 1 (X or value)
    float param2;               // +0x0C - Parameter 2 (Y or value)
    float param3;               // +0x10 - Parameter 3 (Z or value)
    float param4;               // +0x14 - Parameter 4 (extra)
};
```

## Action Types

| Type | Function Called        | Description           |
|------|------------------------|-----------------------|
| 2    | `sub_4CC5F0`           | Effect type A         |
| 3    | `sub_4CD800`           | Effect type B         |
| 4    | `sub_4C2550`           | Effect type C         |
| 5    | `sub_4C2FA0`           | Effect type D         |
| 7    | `sub_4C7B80`           | Effect type E         |
| 8    | `sub_4CB160(idx, 0)`   | Effect type F         |
| 9    | `sub_4CB6E0`           | Effect type G         |
| 11+  | Various                | Other effects         |

## Handler Logic (IDA)

```c
// sub_47A110
char __stdcall sub_47A110(int a1)
{
    int playerId, actionType;
    int param1, param2, param3, param4;
    
    sub_44E910(a1, &playerId, 4);
    int idx = sub_48DEA0(byte_1B19090, playerId);
    
    if (idx >= 0) {
        sub_44E910(a1, &actionType, 4);
        sub_44E910(a1, &param1, 4);
        sub_44E910(a1, &param2, 4);
        sub_44E910(a1, &param3, 4);
        sub_44E910(a1, &param4, 4);
        
        switch (actionType) {
            case 2:
                sub_4CC5F0(byte_2F03268, idx, 
                           *(float*)&param1, *(float*)&param2, 
                           *(float*)&param3, param4);
                break;
            case 3:
                sub_4CD800(byte_2F05F90, idx, 
                           *(float*)&param1, *(float*)&param2, 
                           *(float*)&param3, *(float*)&param4);
                break;
            // ... more cases
        }
    }
}
```

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_47A110     | 24 bytes     |
| Ghidra | FUN_0047a110   | 24 bytes     |

**Status**: ✅ CERTIFIED
