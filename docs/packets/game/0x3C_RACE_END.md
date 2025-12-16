# 0x3C - RACE_END

**CMD**: `0x3C` (60 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47A5C0`  
**Handler Ghidra**: `FUN_0047a5c0`

## Description

Notifies when a player finishes a race. Contains the player's final position data, finish status, and rank.

## Payload Structure

| Offset | Type   | Size | Description                |
|--------|--------|------|----------------------------|
| 0x00   | int32  | 4    | Player ID                  |
| 0x04   | int32  | 4    | Unknown (stored at +0x664) |
| 0x08   | byte   | 1    | Unknown (stored at +0x659) |
| 0x09   | int32  | 4    | Unknown (stored at +0x65C) |
| 0x0D   | int32  | 4    | Result flag (stored at +0x1B0) |

**Total Size**: 17 bytes

## C Structure

```c
struct RaceEndPacket {
    int32_t playerId;           // +0x00 - Player who finished
    int32_t unknown1;           // +0x04 - Stored at offset +0x664
    uint8_t unknown2;           // +0x08 - Stored at offset +0x659
    int32_t unknown3;           // +0x09 - Stored at offset +0x65C
    int32_t resultFlag;         // +0x0D - Stored at offset +0x1B0
};
```

## Handler Logic (IDA)

```c
// sub_47A5C0
char __thiscall sub_47A5C0(void *this, int a2)
{
    int v2 = a2;
    
    sub_44E910(a2, &a2, 4);                              // Read playerId
    sub_44E910(v2, (char*)&dword_80E664 + this, 4);      // Read unknown1
    sub_44E910(v2, &byte_80E659[this], 1);               // Read unknown2
    sub_44E910(v2, (char*)&dword_80E65C + this, 4);      // Read unknown3
    sub_44E910(v2, (char*)&dword_80E1B0 + this, 4);      // Read resultFlag
    
    int idx = sub_48DEA0(byte_1B19090, a2);  // Find player index
    if (idx >= 0) {
        sub_499030(byte_1B19090, idx, 1);
        sub_48B460(dword_1AF2BB0, dword_1B1C5B0[171160 * idx], 9);
        if (a2 == dword_1A20658)  // If local player
            sub_4815F0(dword_12124B0, 9);
    }
    
    // Play sound based on result
    if (*v4)  // resultFlag != 0
        sub_448C90(byte_E22080, dword_B2318C, 0);  // Win sound
    else
        sub_448C90(byte_E22080, dword_B23188, 0);  // Lose sound
    
    return sub_43ED70(&off_5C8310, 9, -1, 1065353216);
}
```

## Cross-Validation

| Source | Function       | Payload Read      |
|--------|----------------|-------------------|
| IDA    | sub_47A5C0     | 4+4+1+4+4 = 17 bytes |
| Ghidra | FUN_0047a5c0   | 4+4+1+4+4 = 17 bytes |

**Status**: ✅ CERTIFIED
