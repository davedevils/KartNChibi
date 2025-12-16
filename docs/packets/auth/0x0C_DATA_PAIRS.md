# 0x0C DATA_PAIRS (Server → Client)

**Handler:** `sub_4791A0` / `FUN_004791a0`  
**Status:** ✅ CERTIFIÉ IDA+Ghidra

## Purpose

Sends a list of key-value pairs.

## Payload Structure

```c
struct DataPairs {
    int32 count;
    struct {
        int32 key;
        int32 value;
    } pairs[count];
};
```

## Size

**4 + 8N bytes** (where N = count)

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Number of pairs |
| 0x04 | int32 | 4 | Key 1 |
| 0x08 | int32 | 4 | Value 1 |
| ... | ... | ... | ... |

## Notes

- Variable size based on count
- Maximum handled pairs limited by client
- Used for bulk data updates

