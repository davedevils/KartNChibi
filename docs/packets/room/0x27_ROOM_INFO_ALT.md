# 0x27 - ROOM_INFO_ALT

**CMD**: `0x27` (39 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_479A30`  
**Handler Ghidra**: `FUN_00479a30`

## Description

Alternative room info packet. Contains an integer followed by string data.

## Payload Structure

| Offset | Type    | Size     | Description                    |
|--------|---------|----------|--------------------------------|
| 0x00   | int32   | 4        | Unknown value (set to 0 after read) |
| 0x04   | wstring | variable | Wide string (UTF-16LE, null-term) |
| ?      | string  | variable | ASCII string (null-term)       |

**Total Size**: Variable (4 + wstring + string)

## C Structure

```c
struct RoomInfoAltPacket {
    int32_t unknown;            // +0x00 - Value (discarded, set to 0)
    wchar_t wideStr[];          // UTF-16LE null-terminated
    // After wstring:
    char asciiStr[];            // ASCII null-terminated
};
```

## Handler Logic (IDA)

```c
// sub_479A30
int __stdcall sub_479A30(const wchar_t **a1)
{
    int v3;         // value (set to 0 after reading)
    char v4[28];    // wstring buffer
    char v5[256];   // ASCII string buffer
    
    sub_44E910(a1, &v3, 4);     // Read int32
    sub_44EB60(a1, v4);         // Read wstring
    sub_44EB30(a1, v5);         // Read ASCII string
    
    v3 = 0;  // Value is discarded!
    return nullsub_8(v5, v4, 1);
}
```

## Cross-Validation

| Source | Function       | Payload Read           |
|--------|----------------|------------------------|
| IDA    | sub_479A30     | 4 + wstring + string   |
| Ghidra | FUN_00479a30   | 4 + wstring + string   |

**Status**: ✅ CERTIFIED
