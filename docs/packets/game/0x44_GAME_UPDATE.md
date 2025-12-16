# 0x44 - GAME_UPDATE

**CMD**: `0x44` (68 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47A0E0`  
**Handler Ghidra**: `FUN_0047a0e0`

## Description

Updates game state. Calls `sub_4B0CF0` with the received value.

## Payload Structure

| Offset | Type   | Size | Description           |
|--------|--------|------|-----------------------|
| 0x00   | int32  | 4    | Update value          |

**Total Size**: 4 bytes

## C Structure

```c
struct GameUpdatePacket {
    int32_t value;              // +0x00 - Update value
};
```

## Handler Logic (IDA)

```c
// sub_47A0E0
void __stdcall sub_47A0E0(int a1)
{
    sub_44E910(a1, &a1, 4);  // Read value
    sub_4B0CF0(byte_2EB89A0, a1);
}
```

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_47A0E0     | 4 bytes      |
| Ghidra | FUN_0047a0e0   | 4 bytes      |

**Status**: ✅ CERTIFIED
