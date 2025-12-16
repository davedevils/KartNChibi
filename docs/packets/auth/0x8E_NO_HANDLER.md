# 0x8E NO_HANDLER (Server → Client)

**Packet ID:** 0x8E (142 decimal)
**Handler:** **NONE**

## ⚠️ WARNING

**This packet has NO HANDLER in the client!**

The client's packet switch does not include `case 142:`, so this packet is silently ignored.

## Verification (IDA)

```c
// In main packet handler sub_4777C0:
// case 142: NOT FOUND!
// 
// Packets go from case 140 to case 143:
//   case 140: sub_47B9E0(...)
//   case 143: sub_47E950(...)  // 0x8F
//   case 144: sub_47DF30(...)  // 0x90
```

## Notes

- Do NOT send this packet - client will ignore it
- If you need an ACK, use a different packet (0x0B, etc.)
- Previous server implementations using 0x8E were wrong

