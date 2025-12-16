# 0x68 - SHOP_LOOKUP

**CMD**: `0x68` (104 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47AE30`  
**Handler Ghidra**: `FUN_0047ae30`

## Description

Updates shop-related position data for a player. Stores coordinate values in multiple memory locations.

## Payload Structure

| Offset | Type   | Size | Description                  |
|--------|--------|------|------------------------------|
| 0x00   | int32  | 4    | Player ID                    |
| 0x04   | int32  | 4    | X coordinate (at +0x2D4)     |
| 0x08   | int32  | 4    | Y coordinate (at +0x2D8)     |
| 0x0C   | int32  | 4    | Z coordinate (at +0x2DC)     |
| 0x10   | int32  | 4    | Unknown (at +0x2B0)          |

**Total Size**: 20 bytes

## C Structure

```c
struct ShopLookupPacket {
    int32_t playerId;           // +0x00 - Target player ID
    int32_t posX;               // +0x04 - X coordinate
    int32_t posY;               // +0x08 - Y coordinate
    int32_t posZ;               // +0x0C - Z coordinate
    int32_t unknown;            // +0x10 - Unknown value
};
```

## Handler Logic (IDA)

```c
// sub_47AE30
void __stdcall sub_47AE30(int a1)
{
    int playerId;
    
    sub_44E910(a1, &playerId, 4);  // Read playerId
    int idx = sub_48DEA0(byte_1B19090, playerId);
    
    if (idx >= 0) {
        int offset = 684640 * idx;
        int *v4 = &dword_1B1C2D4[171160 * idx];
        
        sub_44E910(a1, v4, 4);                   // Read X
        sub_44E910(a1, &dword_1B1C2D8[offset/4], 4);  // Read Y
        sub_44E910(a1, &dword_1B1C2DC[offset/4], 4);  // Read Z
        sub_44E910(a1, &dword_1B1C2B0[offset/4], 4);  // Read unknown
        
        // Copies values to multiple memory locations
        // (0x914, 0x920, 0x92C offsets)
    }
}
```

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_47AE30     | 20 bytes     |
| Ghidra | FUN_0047ae30   | 20 bytes     |

**Status**: ✅ CERTIFIED
