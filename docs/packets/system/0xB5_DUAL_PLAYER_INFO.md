# 0xB5 - DUAL_PLAYER_INFO

**CMD**: `0xB5` (181 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47CF80`  
**Handler Ghidra**: `FUN_0047cf80`

## Description

Sends two PlayerInfo structures followed by a wstring. Used for comparison/display of two players.

## Payload Structure

| Offset | Type       | Size  | Description           |
|--------|------------|-------|-----------------------|
| 0x00   | PlayerInfo | 1224  | First PlayerInfo      |
| 0x4C8  | PlayerInfo | 1224  | Second PlayerInfo     |
| 0x990  | wstring    | var   | Additional string     |

**Total Size**: 2448 + wstring bytes

## C Structure

```c
struct DualPlayerInfoPacket {
    PlayerInfo player1;         // +0x000 - First player (1224 bytes)
    PlayerInfo player2;         // +0x4C8 - Second player (1224 bytes)
    // wstring additionalData;  // After both PlayerInfo
};
```

## Handler Logic (IDA)

```c
// sub_47CF80
char __thiscall sub_47CF80(void *this, const wchar_t **a2)
{
    int v10[289];  // 1156 bytes for partial PlayerInfo
    int v14[307];  // 1228 bytes
    char v13[512];
    
    sub_44E910((int)a2, v10, 0x4C8);   // Read first PlayerInfo
    sub_44E910((int)a2, v14, 0x4C8);   // Read second PlayerInfo
    sub_44EB60(a2, v13);              // Read wstring
    
    // ... comparison logic for 5 player slots
}
```

## Notes

- Used for comparing two players' stats/info
- Contains complex UI logic for managing player slots
- The wstring may contain formatted display text

## Cross-Validation

| Source | Function       | Payload Read         |
|--------|----------------|----------------------|
| IDA    | sub_47CF80     | 0x4C8 + 0x4C8 + wstr |
| Ghidra | FUN_0047cf80   | 0x4C8 + 0x4C8 + wstr |

**Status**: ✅ CERTIFIED

