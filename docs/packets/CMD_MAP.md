# Complete CMD → Handler Mapping

Extracted from `sub_4777C0` (main packet dispatcher)

## CMD Table

| CMD | Hex | Handler | Category | Notes |
|-----|-----|---------|----------|-------|
| 1 | 0x01 | sub_478DA0 | Auth | Login response |
| 2 | 0x02 | sub_478D40 | Auth | Post-login |
| 3 | 0x03 | sub_479220 | Auth | Unknown |
| 4 | 0x04 | sub_479230 | Auth | Unknown |
| 7 | 0x07 | sub_479020 | Auth | Session info |
| **8** | 0x08 | **nullsub** | - | Ignored |
| 10 | 0x0A | sub_47D3B0 | Auth | Connection OK |
| 11 | 0x0B | sub_479190 | UI | Unknown |
| 12 | 0x0C | sub_4791A0 | UI | Unknown |
| 13 | 0x0D | sub_47ADE0 | UI | Unknown |
| 14 | 0x0E | sub_4793F0 | UI | Show Menu |
| 15 | 0x0F | sub_479520 | UI | Show Garage |
| 16 | 0x10 | sub_479550 | UI | Show Shop |
| 17 | 0x11 | sub_4793C0 | UI | Show Lobby |
| 18 | 0x12 | sub_4795A0 | UI | Show Room / Heartbeat resp |
| 19 | 0x13 | sub_47FC20 | UI | Unknown |
| 20 | 0x14 | sub_479CC0 | Game | Race/Game mode |
| 22 | 0x16 | sub_47E8B0 | Game | Unknown |
| 24 | 0x18 | sub_47C7A0 | Game | Unknown |
| 25 | 0x19 | sub_479340 | Client | Server query |
| 27 | 0x1B | sub_478EC0 | Inventory | Inventory A |
| 28 | 0x1C | sub_478F20 | Inventory | Inventory B |
| 29 | 0x1D | sub_478F80 | Inventory | Inventory C |
| 30 | 0x1E | sub_478FE0 | Inventory | Item update |
| **31-32** | 0x1F-20 | **nullsub** | - | Ignored |
| 33 | 0x21 | sub_4797C0 | Room | Room full |
| 34 | 0x22 | sub_479920 | Room | Leave room |
| 35 | 0x23 | sub_479BE0 | Room | Player update |
| **36** | 0x24 | **nullsub** | - | Ignored |
| 37 | 0x25 | sub_479AB0 | Room | Unknown |
| 39 | 0x27 | sub_479A30 | Room | Unknown |
| 40 | 0x28 | sub_479B20 | Room | Unknown |
| 42 | 0x2A | sub_479B60 | Room | Unknown |
| 43 | 0x2B | sub_479BD0 | Room | Unknown |
| 45 | 0x2D | sub_479630 | Chat | Chat message |
| 46 | 0x2E | sub_479710 | Chat | Player left |
| 48 | 0x30 | sub_479760 | Room | Room state |
| 49 | 0x31 | sub_479950 | Game | Position |
| 50 | 0x32 | sub_479C70 | Game | Unknown |
| 51 | 0x33 | sub_4799B0 | Game | Game state |
| 52 | 0x34 | sub_479A10 | Game | Unknown |
| 53 | 0x35 | sub_479C20 | Game | Score |
| **54-56** | 0x36-38 | **nullsub** | - | Ignored |
| 57 | 0x39 | sub_479CB0 | Game | Finish |
| 58 | 0x3A | sub_47AE00 | Game | Results |
| 60 | 0x3C | sub_47A5C0 | Game | Race end |
| 61 | 0x3D | sub_47A6B0 | Room | Unknown |
| 62 | 0x3E | sub_479D60 | Room | Player join |
| 63 | 0x3F | sub_47A050 | Room | Room info |
| 64 | 0x40 | sub_47FD30 | Game | Unknown |
| 66 | 0x42 | sub_47A0A0 | Game | Unknown |
| **67** | 0x43 | **nullsub** | - | Ignored |
| 68 | 0x44 | sub_47A0E0 | Game | Unknown |
| 69 | 0x45 | sub_47A560 | Game | Unknown |
| 70 | 0x46 | sub_47A760 | Game | Unknown |
| 71 | 0x47 | sub_47A110 | Game | Unknown |
| **72** | 0x48 | **nullsub** | - | Ignored |
| 73 | 0x49 | sub_47AAD0 | Game | Unknown |
| 75 | 0x4B | sub_47A460 | Game | Unknown |
| 77 | 0x4D | sub_4790C0 | Client | Request data |
| 78 | 0x4E | sub_479160 | Game | Checkpoint? |
| 84 | 0x54 | sub_47AA00 | Game | Unknown |
| 87 | 0x57 | sub_47AB40 | Game | Unknown |
| 88 | 0x58 | sub_47ABE0 | Game | Unknown |
| 92 | 0x5C | sub_47A500 | Room | Unknown |
| 95 | 0x5F | sub_47EE70 | Game | Unknown |
| 98 | 0x62 | sub_479580 | Room | Tutorial fail |
| 99 | 0x63 | sub_47AC30 | Room | Create room |
| 100 | 0x64 | sub_47ACD0 | Room | Unknown |
| 101 | 0x65 | sub_47AD60 | Room | Unknown |
| **102** | 0x66 | **nullsub** | - | Ignored |
| 104 | 0x68 | sub_47AE30 | Shop | Unknown |
| 105 | 0x69 | sub_47AF00 | Shop | Unknown |
| 106 | 0x6A | sub_47AFE0 | Shop | Unknown |
| 108 | 0x6C | sub_47B030 | Shop | Unknown |
| **109** | 0x6D | **nullsub** | - | Ignored |
| 110 | 0x6E | sub_47B190 | Shop | Unknown |
| 111 | 0x6F | sub_47B3C0 | Shop | Shop response |
| 112-113 | 0x70-71 | sub_47B300 | Shop | Same handler |
| 114 | 0x72 | sub_47B550 | Shop | Data block |
| 115 | 0x73 | sub_47B570 | Shop | Slot update |
| 116 | 0x74 | sub_47B620 | Shop | Unknown |
| 118 | 0x76 | sub_47B1B0 | Inventory | Unknown |
| 119 | 0x77 | sub_47B340 | Inventory | Unknown |
| 120 | 0x78 | sub_47B220 | Inventory | Item list |
| 121 | 0x79 | sub_47B290 | Inventory | Item list B |
| 122 | 0x7A | sub_47B4F0 | Inventory | Item add |
| 123 | 0x7B | sub_47B670 | Inventory | Unknown |
| 125 | 0x7D | sub_47B6B0 | Inventory | Unknown |
| **126-128** | 0x7E-80 | **nullsub** | - | Ignored |
| 129 | 0x81 | sub_47B8A0 | Misc | Unknown |
| 130 | 0x82 | sub_47B710 | Misc | Unknown |
| 131 | 0x83 | sub_47B7D0 | Misc | Unknown |
| 132 | 0x84 | sub_47B8D0 | Misc | Unknown |
| 133 | 0x85 | sub_47B900 | Misc | Unknown |
| **134** | 0x86 | **nullsub** | - | Ignored |
| 135 | 0x87 | sub_47DC10 | Misc | Unknown |
| 136 | 0x88 | sub_47B930 | Misc | Unknown |
| 138 | 0x8A | sub_47B990 | Misc | Unknown |
| **139, 141** | 0x8B, 8D | **nullsub** | - | Ignored |
| 140 | 0x8C | sub_47B9E0 | Misc | Unknown |
| 143 | 0x8F | sub_47E950 | Auth | Ack 0x07 |
| 144 | 0x90 | sub_47DF30 | Auth | Unknown |
| **145-148** | 0x91-94 | **nullsub** | - | Ignored |
| 149, 151 | 0x95, 97 | sub_47BE00 | Misc | Same handler |
| 150 | 0x96 | sub_47BD80 | Misc | Unknown |
| 152 | 0x98 | sub_47BEF0 | Misc | Unknown |
| 153 | 0x99 | sub_47BE80 | Misc | Unknown |
| 154 | 0x9A | sub_47BF80 | Misc | Unknown |
| 155 | 0x9B | sub_47C270 | Misc | Unknown |
| 156 | 0x9C | sub_47C2A0 | Misc | Unknown |
| 157 | 0x9D | sub_47C2D0 | Misc | Unknown |
| 158 | 0x9E | sub_47C320 | Misc | Unknown |
| 159 | 0x9F | sub_47C370 | Misc | Unknown |
| **160** | 0xA0 | **nullsub** | - | Ignored |
| 161 | 0xA1 | sub_47C930 | Misc | Unknown |
| 162 | 0xA2 | sub_47C960 | Misc | Unknown |
| 163 | 0xA3 | sub_47C3C0 | Misc | Unknown |
| 164 | 0xA4 | sub_47C770 | Misc | Unknown |
| 165 | 0xA5 | sub_47C830 | Misc | Unknown |
| 167 | 0xA7 | sub_479080 | Auth | Session confirm |
| 170 | 0xAA | sub_47CBA0 | Misc | Unknown |
| 171 | 0xAB | sub_47C9C0 | Misc | Unknown |
| 172 | 0xAC | sub_47C9F0 | Misc | Unknown |
| 173 | 0xAD | sub_47CAE0 | Misc | Unknown |
| 174 | 0xAE | sub_47CB00 | Misc | Unknown |
| 175 | 0xAF | sub_47CB30 | Misc | Unknown |
| 176 | 0xB0 | sub_47CC20 | Misc | Unknown |
| 180 | 0xB4 | sub_47CD60 | Misc | Unknown |
| 181 | 0xB5 | sub_47CF80 | Misc | Unknown |
| 182 | 0xB6 | sub_47AC60 | Misc | Unknown |
| 183 | 0xB7 | sub_484F50 | Misc | Unknown |
| 184 | 0xB8 | sub_484690 | Misc | Unknown |
| 185 | 0xB9 | sub_484770 | Misc | Unknown |
| 186 | 0xBA | sub_484B10 | Misc | Unknown |
| **187** | 0xBB | **nullsub** | - | Ignored |

## Ignored CMDs (nullsub)

These CMDs are explicitly ignored:
```
8, 31, 32, 36, 54, 55, 56, 67, 72, 102, 109, 
126, 127, 128, 134, 139, 141, 145, 146, 147, 148, 160, 187
```

In hex:
```
0x08, 0x1F, 0x20, 0x24, 0x36, 0x37, 0x38, 0x43, 0x48, 0x66, 0x6D,
0x7E, 0x7F, 0x80, 0x86, 0x8B, 0x8D, 0x91, 0x92, 0x93, 0x94, 0xA0, 0xBB
```

## Notes

- CMD is obtained via `sub_44E8F0` (reads CMD byte from packet)
- Handlers receive `CompletionKey` (connection context) and `a2` (packet data)
- Many handlers in 0x87+ range are for extended features (missions, rankings, etc.)

