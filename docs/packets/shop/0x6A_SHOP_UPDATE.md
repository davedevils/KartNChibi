# 0x6A - SHOP_UPDATE

**CMD**: `0x6A` (106 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47AFE0`  
**Handler Ghidra**: `FUN_0047afe0`

## Description

Updates shop-related state for a player. Calls `sub_49E8A0` with the player index and value.

## Payload Structure

| Offset | Type   | Size | Description           |
|--------|--------|------|-----------------------|
| 0x00   | int32  | 4    | Player ID             |
| 0x04   | int16  | 2    | Update value          |

**Total Size**: 6 bytes

## C Structure

```c
struct ShopUpdatePacket {
    int32_t playerId;           // +0x00 - Target player ID
    int16_t value;              // +0x04 - Update value
};
```

## Handler Logic (IDA)

```c
// sub_47AFE0
int __stdcall sub_47AFE0(int a1)
{
    int playerId;
    int16_t value;
    
    sub_44E910(a1, &playerId, 4);  // Read playerId
    int idx = sub_48DEA0(byte_1B19090, playerId);
    
    if (idx >= 0) {
        sub_44E910(a1, &value, 2);  // Read value
        return sub_49E8A0(byte_1B19090, idx, value);
    }
    return idx;
}
```

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_47AFE0     | 6 bytes      |
| Ghidra | FUN_0047afe0   | 6 bytes      |

**Status**: ✅ CERTIFIED
