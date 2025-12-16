# 0xAA - PLAYER_DATA

**CMD**: `0xAA` (170 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47CBA0`  
**Handler Ghidra**: `FUN_0047cba0`

## Description

Sends player data including vehicle and item information. Triggers UI state 15.

## Payload Structure

| Offset | Type      | Size | Description           |
|--------|-----------|------|-----------------------|
| 0x00   | int32     | 4    | Player ID             |
| 0x04   | wstring   | var  | Player name           |
| +      | int32     | 4    | Unknown (stored @0x938)|
| +      | int32     | 4    | Unknown (stored @0x93C)|
| +      | VehicleData| 44  | Vehicle data (0x2C)   |
| +      | ItemData  | 56   | Item data (0x38)      |

**Total Size**: Variable (60 + wstring minimum)

## C Structure

```c
struct PlayerDataPacket {
    int32_t playerId;           // +0x00
    // wstring playerName;      // Variable length
    int32_t unknown1;           // After name
    int32_t unknown2;           // 
    VehicleData vehicle;        // 44 bytes
    ItemData item;              // 56 bytes
};
```

## Handler Logic (IDA)

```c
// sub_47CBA0
char __stdcall sub_47CBA0(const wchar_t **a1)
{
    sub_44E910((int)a1, &dword_C1A9C8, 4);      // Read int32
    sub_44EB60(a1, &unk_C1A940);                // Read wstring
    sub_44E910((int)a1, &unk_C1A938, 4);        // Read int32
    sub_44E910((int)a1, &dword_C1A93C, 4);      // Read int32
    sub_44E910((int)a1, &unk_C1A95C, 0x2C);     // Read VehicleData
    sub_44E910((int)a1, &unk_C1A988, 0x38);     // Read ItemData
    
    sub_4538B0();
    return sub_404410(dword_B23288, 15);        // Set UI state 15
}
```

## Cross-Validation

| Source | Function       | Payload Read                    |
|--------|----------------|---------------------------------|
| IDA    | sub_47CBA0     | 4 + wstring + 4 + 4 + 44 + 56   |
| Ghidra | FUN_0047cba0   | 4 + wstring + 4 + 4 + 44 + 56   |

**Status**: ✅ CERTIFIED

