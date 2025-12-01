# CMD 0x74 (116) - Unknown

**Direction:** Server → Client  
**Handler:** `sub_47B620`

## Structure

```c
struct Unknown74 {
    int32 value1;
    int32 value2;
};
```

## Fields

| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | 4 | int32 | value1 |
| 0x04 | 4 | int32 | value2 |

## Behavior

1. Reads value1
2. Reads value2
3. Calls `sub_44F0E0` with value1
4. Sets flags:
   - `byte_F78F60 = 1`
   - `dword_F78F64 = value2`

## Raw Packet

```
74 08 00 00 00 00 00 00
01 00 00 00              // value1 = 1
02 00 00 00              // value2 = 2
```

## Notes

- Simple 2-value packet
- Sets shop-related flags
- Exact purpose unclear

