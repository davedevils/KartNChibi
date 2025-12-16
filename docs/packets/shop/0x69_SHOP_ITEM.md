# 0x69 - SHOP_ITEM

**CMD**: `0x69` (105 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47AF00`  
**Handler Ghidra**: `FUN_0047af00`

## Description

Triggers shop-related actions based on a type code. Different codes trigger different shop subsystems.

## Payload Structure

| Offset | Type   | Size | Description           |
|--------|--------|------|-----------------------|
| 0x00   | int32  | 4    | Player ID             |
| 0x04   | int16  | 2    | Action type code      |
| 0x06   | byte   | 1    | Unknown flag          |

**Total Size**: 7 bytes

## C Structure

```c
struct ShopItemPacket {
    int32_t playerId;           // +0x00 - Target player ID
    int16_t actionType;         // +0x04 - Action type code
    uint8_t flag;               // +0x06 - Unknown flag
};
```

## Action Type Codes

| Code | Action                              |
|------|-------------------------------------|
| 100  | `sub_495C30(idx, 100)` - Type A     |
| 200  | `sub_495C30(idx, 200)` - Type B     |
| 300  | `sub_495C30(idx, 300)` - Type C     |
| 700  | `sub_4C3A60(idx)` - Special action  |
| 1000 | `sub_4C4A10(idx)` - Premium action  |

## Handler Logic (IDA)

```c
// sub_47AF00
void __stdcall sub_47AF00(int a1)
{
    int playerId;
    int16_t actionType;
    int8_t flag;
    
    sub_44E910(a1, &playerId, 4);
    int idx = sub_48DEA0(byte_1B19090, playerId);
    
    if (idx >= 0) {
        sub_44E910(a1, &actionType, 2);
        sub_44E910(a1, &flag, 1);
        
        switch (actionType) {
            case 100:
            case 200:
            case 300:
                sub_495C30(byte_1B19090, idx, actionType);
                break;
            case 700:
                sub_4C3A60(dword_2EF3000, idx);
                break;
            case 1000:
                sub_4C4A10(dword_2EF48E8, idx);
                break;
        }
    }
}
```

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_47AF00     | 7 bytes      |
| Ghidra | FUN_0047af00   | 7 bytes      |

**Status**: ✅ CERTIFIED
