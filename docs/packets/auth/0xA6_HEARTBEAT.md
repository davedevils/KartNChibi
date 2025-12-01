# CMD 0xA6 - Heartbeat / Login Poll

**Direction:** Client → Server  
**Builder:** `sub_477610`

## Structure

```c
struct Heartbeat {
    int32 sessionData;    // Session identifier or timestamp
};
```

## Fields

| Offset | Size | Type | Name | Description |
|--------|------|------|------|-------------|
| 0x00 | 4 | int32 | sessionData | From client context (0x80E07C) |

## Raw Packet

```
04 00 A6 00 00 00 00 00 XX XX XX XX
│  │  │               │  └─────────── sessionData (4 bytes)
│  │  │               └───────────── flags (5 bytes, zeros)
│  │  └───────────────────────────── CMD = 0xA6
│  └──────────────────────────────── size = 4 (payload only)
└─────────────────────────────────── size high byte
```

## Rate Limiting

- **Minimum interval:** 1000ms (0x3E8)
- Client checks timestamp before sending
- Won't send if less than 1 second since last send

## Client Code

```c
uint32 now = GetTick();
uint32 lastSend = ctx->lastSendTime;  // offset 0x8CCDB0

if ((now - lastSend) >= 0x3E8) {  // 1000ms
    ctx->lastSendTime = now;
    
    Packet pkt;
    pkt.cmd = 0xA6;
    pkt.WriteInt32(ctx->sessionData);  // offset 0x80E07C
    Send(pkt);
    
    ctx->sessionData = 0;  // Reset after send
}
```

## Server Response

Server should respond with one of:
- **CMD 0x12** (Show Lobby) - Keep connection alive
- **CMD 0x01** (Login Response) - If re-authentication needed
- **CMD 0x07/0xA7** - If session data requested

## Notes

- This is NOT spam - client rate limits to 1/second
- Acts as both heartbeat and login polling
- Session data is cleared after each send
- Server should track last heartbeat for timeout detection

