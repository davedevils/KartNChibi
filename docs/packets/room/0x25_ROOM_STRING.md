# 0x25 - ROOM_STRING

**CMD**: `0x25` (37 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_479AB0`  
**Handler Ghidra**: `FUN_00479ab0`

## Description

Sends room-related string data. Contains a wide string, ASCII string, and an integer value.

## Payload Structure

| Offset | Type    | Size     | Description                    |
|--------|---------|----------|--------------------------------|
| 0x00   | wstring | variable | Wide string (UTF-16LE, null-term) |
| ?      | string  | variable | ASCII string (null-term)       |
| ?      | int32   | 4        | Value (passed as v4+1)         |

**Total Size**: Variable (wstring + string + 4)

## C Structure

```c
struct RoomStringPacket {
    wchar_t wideStr[];          // UTF-16LE null-terminated
    // After wstring:
    char asciiStr[];            // ASCII null-terminated
    // After string:
    int32_t value;              // Value parameter
};
```

## Handler Logic (IDA)

```c
// sub_479AB0
int __stdcall sub_479AB0(const wchar_t **a1)
{
    char v5[28];    // wstring buffer
    char v6[256];   // ASCII string buffer
    int v4;         // value
    
    sub_44EB60(a1, v5);         // Read wstring
    sub_44EB30(a1, v6);         // Read ASCII string
    sub_44E910(a1, &v4, 4);     // Read int32
    
    return nullsub_8(v6, v5, v4 + 1);
}
```

## Cross-Validation

| Source | Function       | Payload Read           |
|--------|----------------|------------------------|
| IDA    | sub_479AB0     | wstring + string + 4   |
| Ghidra | FUN_00479ab0   | wstring + string + 4   |

**Status**: ✅ CERTIFIED
