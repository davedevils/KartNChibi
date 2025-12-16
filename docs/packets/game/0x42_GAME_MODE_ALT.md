# 0x42 - GAME_MODE_ALT

**CMD**: `0x42` (66 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47A0A0`  
**Handler Ghidra**: `FUN_0047a0a0`

## Description

Alternative game mode control packet. Sets the current game mode/state.

## Payload Structure

| Offset | Type   | Size | Description           |
|--------|--------|------|-----------------------|
| 0x00   | int32  | 4    | Mode value            |

**Total Size**: 4 bytes

## C Structure

```c
struct GameModeAltPacket {
    int32_t mode;               // +0x00 - Game mode value
};
```

## Mode Values

| Value | Action                            |
|-------|-----------------------------------|
| 5     | Calls `sub_4B6220` (special mode) |
| Other | Calls `sub_4B23C0(mode, 1)`       |

## Handler Logic (IDA)

```c
// sub_47A0A0
char __stdcall sub_47A0A0(int a1)
{
    sub_44E910(a1, &a1, 4);  // Read mode
    
    if (a1 == 5)
        return sub_4B6220(dword_2EBFD40);
    else
        return sub_4B23C0(dword_2EB9B90, a1, 1);
}
```

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_47A0A0     | 4 bytes      |
| Ghidra | FUN_0047a0a0   | 4 bytes      |

**Status**: ✅ CERTIFIED

