# CMD 0x73 (115) - Slot Update

**Direction:** Server → Client  
**Handler:** `sub_47B570`

## Structure

```c
struct SlotUpdate {
    int32 count;
    SlotEntry entries[count];
};

struct SlotEntry {
    int32 slotId;
    int32 value1;
    int32 value2;
};
```

## Fields

### Header
| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | 4 | int32 | count |

### Per Entry (12 bytes = 0x0C)
| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | 4 | int32 | slotId |
| 0x04 | 4 | int32 | value1 |
| 0x08 | 4 | int32 | value2 |

## Behavior

1. Iterates existing slots, sets values to -2
2. Reads new entries
3. For each entry:
   - Looks up slot by slotId
   - Sets offset +36 to value1
   - Sets offset +40 to value2

## Raw Packet

```
73 10 00 00 00 00 00 00
02 00 00 00              // count = 2
// Entry 1
01 00 00 00              // slotId = 1
0A 00 00 00              // value1 = 10
14 00 00 00              // value2 = 20
// Entry 2
02 00 00 00              // slotId = 2
05 00 00 00              // value1 = 5
0F 00 00 00              // value2 = 15
```

## Notes

- Updates inventory slot values
- -2 used as "unset" marker

