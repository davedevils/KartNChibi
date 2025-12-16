# 0x98 - GIFT

**CMD**: `0x98` (152 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47BEF0`  
**Handler Ghidra**: `FUN_0047bef0`

## Description

Notifies the player of a gift received. Displays "MSG_GIFT_SEND" message.

## Payload Structure

| Offset | Type   | Size | Description           |
|--------|--------|------|-----------------------|
| 0x00   | int32  | 4    | Value A (stored at +0x660) |
| 0x04   | int32  | 4    | Value B (stored at +0x664) |
| 0x08   | bytes  | 212  | Gift data block       |

**Total Size**: 220 bytes (4 + 4 + 0xD4)

## C Structure

```c
struct GiftPacket {
    int32_t valueA;             // +0x00 - Stored at offset +0x660
    int32_t valueB;             // +0x04 - Stored at offset +0x664
    uint8_t giftData[212];      // +0x08 - Gift data (0xD4 bytes)
};
```

## Handler Logic (IDA)

```c
// sub_47BEF0
char __thiscall sub_47BEF0(void *this, int a2)
{
    int v6, v5;
    char v7[212];
    
    sub_44E910(a2, &v6, 4);             // Read value A
    sub_44E910(a2, &v5, 4);             // Read value B
    
    *(int*)((char*)&dword_80E660 + this) = v6;
    *(int*)((char*)&dword_80E664 + this) = v5;
    
    sub_44E910(a2, v7, 0xD4);           // Read 212 bytes
    
    return sub_4641E0(byte_F727E8, "MSG_GIFT_SEND", 1);  // Show message
}
```

## Notes

- Triggers "MSG_GIFT_SEND" localized message display
- The 212-byte gift data contains detailed gift information

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_47BEF0     | 220 bytes    |
| Ghidra | FUN_0047bef0   | 220 bytes    |

**Status**: ✅ CERTIFIED
