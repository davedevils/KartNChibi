# CMD 0x7A (122) - Single Item Add

**Direction:** Server → Client  
**Handler:** `sub_47B4F0`

## Structure

```c
struct SingleItemAdd {
    int32 resultCode;
    // If resultCode == 0:
    uint8 itemData[0x20];  // 32 bytes
};
```

## Fields

| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | 4 | int32 | resultCode |
| 0x04 | 32 | bytes | itemData (only if resultCode == 0) |

## Result Codes

| Code | Meaning |
|------|---------|
| 0 | Success - item data follows |
| != 0 | Error - no additional data |

## Item Data (32 bytes)

Same as ItemEntryB structure from CMD 0x79.

## Behavior

1. Check resultCode
2. If 0: Read 32-byte item, add to inventory
3. If != 0: Error state, no action

## Notes

- Used when acquiring a single item (reward, pickup)
- Result code 0 = success with item data

