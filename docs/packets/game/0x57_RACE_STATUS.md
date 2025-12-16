# 0x57 - RACE_STATUS

**CMD**: `0x57` (87 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47AB40`  
**Handler Ghidra**: `FUN_0047ab40`

## Description

Updates race-specific status for a player. Different status types trigger different race subsystems.

## Payload Structure

| Offset | Type   | Size | Description           |
|--------|--------|------|-----------------------|
| 0x00   | int32  | 4    | Player ID             |
| 0x04   | int32  | 4    | Value                 |
| 0x08   | int32  | 4    | Status type           |

**Total Size**: 12 bytes

## C Structure

```c
struct RaceStatusPacket {
    int32_t playerId;           // +0x00 - Target player ID
    int32_t value;              // +0x04 - Status value
    int32_t statusType;         // +0x08 - Status type
};
```

## Status Types

| Type | Action                                    |
|------|-------------------------------------------|
| 10   | `sub_4C8F60(idx, value)` - Race status A  |
| 16   | `sub_4C59F0(idx, value)` - Race status B  |

## Handler Logic (IDA)

```c
// sub_47AB40
int __thiscall sub_47AB40(void *this, int a2)
{
    int playerId, value, statusType;
    
    sub_44E910(a2, &playerId, 4);
    sub_44E910(a2, &value, 4);
    sub_44E910(a2, &statusType, 4);
    
    int idx = sub_48DEA0(byte_1B19090, playerId);
    if (idx >= 0) {
        if (statusType == 10) {
            // Only process if player matches local player
            if (playerId == *(int*)((char*)&dword_80E1A8 + this))
                return sub_4C8F60(byte_2EFDAB8, idx, value);
        }
        else if (statusType == 16) {
            if (playerId == *(int*)((char*)&dword_80E1A8 + this))
                return sub_4C59F0(byte_2EF61D0, idx, value);
        }
    }
    return idx;
}
```

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_47AB40     | 12 bytes     |
| Ghidra | FUN_0047ab40   | 12 bytes     |

**Status**: ✅ CERTIFIED
