# CMD 0x2D (45) - Chat Message

**Direction:** Bidirectional (Client ↔ Server)

## Structure 

```c
struct ChatMessage {
    int32 senderId;           // Who sent the message
    wchar_t message[42];      // UTF-16LE (84 bytes max)
    int32 padding;            // 0x00000000
    int32 channel;            // 8 = public, 1 = whisper?
    int32 unknown1;           // 0x00000000
    int32 unknown2;           // 0x00000000
    int32 unknown3;           // 0x00000000
    int32 unknown4;           // 0x00000000
    int32 unknown5;           // 0x00000000
};
// Total: 4 + 84 + 32 = 120 bytes
```

## Raw Packets (from capture)

### Example 1: "What kinda Crazy are you?"
```
46 00 2D 00 00 00 00 00     // Size=0x46 (70), CMD=0x2D
57 00 68 00 61 00 74 00     // "What"
20 00 6B 00 69 00 6E 00     // " kin"
64 00 61 00 20 00 43 00     // "da C"
72 00 61 00 7A 00 79 00     // "razy"
20 00 61 00 72 00 65 00     // " are"
20 00 79 00 6F 00 75 00     // " you"
3F 00                       // "?"
00 00                       // null terminator
00 00 00 00                 // unknown1
08 00 00 00                 // channel = 8
00 00 00 00                 // unknown2
00 00 00 00                 // unknown3
00 00 00 00                 // unknown4
```

### Example 2: "Don't know why I didn't race!"
```
4E 00 2D 00 00 00 00 00     // Size=0x4E (78), CMD=0x2D
44 00 6F 00 6E 00 27 00     // "Don'"
74 00 20 00 6B 00 6E 00     // "t kn"
6F 00 77 00 20 00 77 00     // "ow w"
68 00 79 00 20 00 49 00     // "hy I"
20 00 64 00 69 00 64 00     // " did"
6E 00 27 00 74 00 20 00     // "n't "
72 00 61 00 63 00 65 00     // "race"
21 00                       // "!"
00 00                       // null terminator
00 00 00 00                 // unknown1
08 00 00 00                 // channel = 8
00 00 00 00 00 00 00 00 00 00 00 00  // rest
```

## Fields

| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | var | wstring | message (UTF-16LE) |
| var+2 | 4 | int32 | unknown1 (0) |
| var+6 | 4 | int32 | channel |
| var+10 | 4 | int32 | unknown2 (0) |
| var+14 | 4 | int32 | unknown3 (0) |
| var+18 | 4 | int32 | unknown4 (0) |

## Channel Values (speculation)

| Value | Channel |
|-------|---------|
| 0 | System |
| 1 | Whisper |
| 8 | Public/All |

## Notes

- Message is UTF-16LE encoded
- 20 bytes of metadata after null terminator
- Same format for send and receive
- Server broadcasts to other players
