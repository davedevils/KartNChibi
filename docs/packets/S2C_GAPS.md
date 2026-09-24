# S2C gaps, real handlers our server never feeds

## The denominator is not 215

The dispatch table has 215 entries but 23 of them resolve to 0x46F7A0, whose
entire body is one instruction, ret 4. The client accepts those opcodes and
discards them. So
the number that matters is 192 handlers that actually do work.

We emit into 174 of 192, 91 percent. We emit into the ret 4 stub zero times.

Three of the 174 are built with a runtime opcode so no static scan sees them,
0x84 0x85 0x9B, they are counted from the wire.

## The 18 that are left

Bodies resolved through both tables, following only a real trampoline so an SEH
prologue call is never mistaken for one. Reads decoded from 0x44E910 raw,
0x44EB30 cstr, 0x44EB60 wstr. Strings are what the handler references.

| opcode | handler | reads | strings nearby |
|---|---|---|---|
| 0x018 | 0x47C7A0 | u32 cstr u32 u32 |  |
| 0x04D | 0x4790C0 | reads nothing |  |
| 0x099 | 0x47BE80 | raw212 | MSG_UNKNOWN_ERROR |
| 0x09C | 0x47C2A0 | u32 |  |
| 0x0C9 | 0x47CD20 | u32 |  |
| 0x0F3 | 0x47DAA0 | raw156 |  |
| 0x0F5 | 0x47DD10 | u32 u32 |  |
| 0x0F9 | 0x47E350 | raw8 |  |
| 0x116 | 0x47E6D0 | u32 wstr u8 |  |
| 0x117 | 0x47E750 | u32 |  |
| 0x11E | 0x47E9B0 | cstr u32 |  |
| 0x11F | 0x47EA10 | u32 |  |
| 0x120 | 0x47EA70 | u32 raw12 u32 |  |
| 0x122 | 0x47EB70 | reads nothing |  |
| 0x124 | 0x47EB10 | raw52 |  |
| 0x12E | 0x47ED40 | u32 raw16 |  |
| 0x12F | 0x47ED90 | u32 wstr u32 u32 wstr |  |
| 0x131 | 0x478E80 | u32 |  |

## The 23 that are ret 4

0x008 0x01F 0x020 0x024 0x036 0x037 0x038 0x043 0x048 0x066 0x06D 0x07E 0x07F 0x080 0x086 0x08B 0x08D 0x091 0x092 0x093 0x094 0x0A0 0x0BB

Not worth implementing, the client throws them away.

0x36 is in that list. It has no BeginPacket sender either, and a real race
capture has none, so lap complete is dead twice over. Laps stay checkpoint
derived server side.
