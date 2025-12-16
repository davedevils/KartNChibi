# 0x35 - SCORE

**CMD**: `0x35` (53 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_479C20`  
**Handler Ghidra**: `FUN_00479c20`

## Description

Updates the score/lap information during a race. Calls initialization function before processing.

## Payload Structure

| Offset | Type   | Size | Description           |
|--------|--------|------|-----------------------|
| 0x00   | int32  | 4    | Player ID             |
| 0x04   | int32  | 4    | Score / Lap count     |

**Total Size**: 8 bytes

## C Structure

```c
struct ScorePacket {
    int32_t playerId;           // +0x00 - Target player ID
    int32_t score;              // +0x04 - Score or lap count
};
```

## Handler Logic (IDA)

```c
// sub_479C20
int __stdcall sub_479C20(int a1)
{
    int v2;  // score
    int v3;  // playerId
    
    sub_4538B0();               // Initialization call
    sub_44E910(a1, &v3, 4);     // Read playerId
    sub_44E910(a1, &v2, 4);     // Read score
    return sub_410360(dword_B88600, v3, v2);  // Update score
}
```

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_479C20     | 8 bytes      |
| Ghidra | FUN_00479c20   | 8 bytes      |

**Status**: ✅ CERTIFIED
