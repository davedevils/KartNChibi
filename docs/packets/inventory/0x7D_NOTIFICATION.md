# 0x7D - NOTIFICATION

**CMD**: `0x7D` (125 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47B6B0`  
**Handler Ghidra**: `FUN_0047b6b0`

## Description

Displays a notification popup to the player. Contains an ID and a message string.

## Payload Structure

| Offset | Type    | Size     | Description                    |
|--------|---------|----------|--------------------------------|
| 0x00   | int32   | 4        | Notification ID                |
| 0x04   | wstring | variable | Message (UTF-16LE, null-term)  |

**Total Size**: Variable (4 + wstring)

## C Structure

```c
struct NotificationPacket {
    int32_t notificationId;     // +0x00 - Notification ID
    wchar_t message[];          // +0x04 - UTF-16LE null-terminated
};
```

## Handler Logic (IDA)

```c
// sub_47B6B0
unsigned __int8 __stdcall sub_47B6B0(const wchar_t **a1)
{
    sub_44E910(a1, &unk_1211A68, 4);     // Read ID
    sub_44EB60(a1, &unk_1211A6C);        // Read wstring
    
    // Only show popup if not in game (UI state != 11)
    // and certain flags are not set
    if (dword_B2360C != 11) {
        if (byte_11FC19C != 1 && byte_117FEE4 != 1 && byte_1185FAC != 1)
            return sub_476330(&unk_120BBE8);
    }
    return 1;
}
```

## Cross-Validation

| Source | Function       | Payload Read  |
|--------|----------------|---------------|
| IDA    | sub_47B6B0     | 4 + wstring   |
| Ghidra | FUN_0047b6b0   | 4 + wstring   |

**Status**: ✅ CERTIFIED
