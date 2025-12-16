# 0x116 - PLAYER_NAME

**CMD**: `0x116` (278 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47E6D0`  
**Handler Ghidra**: `FUN_0047e6d0`

## Description

Sets a player's display name with an option flag.

## Payload Structure

| Offset | Type    | Size | Description           |
|--------|---------|------|-----------------------|
| 0x00   | int32   | 4    | Player ID             |
| 0x04   | wstring | var  | Player name           |
| +      | byte    | 1    | Option flag           |

**Total Size**: 5 + wstring bytes

## C Structure

```c
struct PlayerNamePacket {
    int32_t playerId;           // +0x00
    // wstring playerName;      // Variable length (UTF-16LE)
    uint8_t optionFlag;         // After wstring
};
```

## Handler Logic (IDA)

```c
// sub_47E6D0
char __stdcall sub_47E6D0(const wchar_t **a1)
{
    int v2[290];
    wchar_t String[25];
    uint8_t flag;
    
    sub_44E910(a1, v2, 4);       // Read player ID
    sub_44EB60(a1, String);     // Read wstring
    sub_44E910(a1, &flag, 1);   // Read flag
    
    return sub_40F550(dword_B9ADD0, v2[0], String, flag);
}
```

## Cross-Validation

| Source | Function       | Payload Read        |
|--------|----------------|---------------------|
| IDA    | sub_47E6D0     | 4 + wstring + 1     |
| Ghidra | FUN_0047e6d0   | 4 + wstring + 1     |

**Status**: ✅ CERTIFIED

