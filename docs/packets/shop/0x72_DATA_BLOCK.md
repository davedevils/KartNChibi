# CMD 0x72 (114) - Shop Data Block

**Direction:** Server → Client  
**Handler:** `sub_47B550`

## Structure

```c
struct DataBlock72 {
    uint8 data[0x68];     // 104 bytes
};
```

## Fields

| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | 104 | bytes | data |

## Behavior

1. Copies 104 bytes to global `unk_F78F68`
2. Sets state flag `dword_F78FD4 = 3`

## Raw Packet

```
72 68 00 00 00 00 00 00
[104 bytes of data]
```

## Notes

- Fixed 104-byte data block
- Purpose unclear - possibly shop configuration
- Sets specific state flag after receiving

