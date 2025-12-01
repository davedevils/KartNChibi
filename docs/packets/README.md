# Packet Reference

Complete packet documentation for KnC protocol.

## Categories

| Category | Description | Packets |
|----------|-------------|---------|
| [auth/](auth/) | Login, session, heartbeat | 14 |
| [ui/](ui/) | Screen transitions | 13 |
| [inventory/](inventory/) | Items, vehicles | 18 |
| [room/](room/) | Room management | 12 |
| [chat/](chat/) | Messaging | 4 |
| [game/](game/) | Race, gameplay | 24 |
| [shop/](shop/) | Transactions | 7 |
| [client/](client/) | Client → Server | 8 |

## Quick Reference

### Server → Client

| CMD | Hex | Name | Category |
|-----|-----|------|----------|
| 1 | 0x01 | Login Response | auth |
| 7 | 0x07 | Player Session | auth |
| 14 | 0x0E | Channel List | ui |
| 15 | 0x0F | Show Garage | ui |
| 16 | 0x10 | Show Shop | ui |
| 17 | 0x11 | Show Menu | ui |
| 18 | 0x12 | Show Lobby | ui |
| 20 | 0x14 | Game Mode | game |
| 27 | 0x1B | Inventory A | inventory |
| 28 | 0x1C | Inventory B | inventory |
| 29 | 0x1D | Inventory C | inventory |
| 30 | 0x1E | Item Update | inventory |
| 33 | 0x21 | Room Info | room |
| 34 | 0x22 | Leave Room | room |
| 35 | 0x23 | Player Update | room |
| 45 | 0x2D | Chat Message | chat |
| 46 | 0x2E | Player Left | chat |
| 48 | 0x30 | Room State | room |
| 49 | 0x31 | Position | game |
| 51 | 0x33 | Game State | game |
| 53 | 0x35 | Score | game |
| 62 | 0x3E | Player Join | room |
| 63 | 0x3F | Player Leave | room |
| 111 | 0x6F | Shop Response | shop |
| 114 | 0x72 | Data Block | shop |
| 115 | 0x73 | Slot Update | shop |
| 116 | 0x74 | Unknown | shop |
| 120 | 0x78 | Item List | inventory |
| 121 | 0x79 | Item List B | inventory |
| 122 | 0x7A | Item Add | inventory |
| 167 | 0xA7 | Login Ack | auth |

### Client → Server

| CMD | Hex | Name | Category |
|-----|-----|------|----------|
| 25 | 0x19 | Server Query | auth |
| 44 | 0x2C | State Change | game |
| 45 | 0x2D | Chat Message | chat |
| 77 | 0x4D | Request Data | misc |
| 115 | 0x73 | Disconnect | auth |
| 166 | 0xA6 | Heartbeat | auth |
| 208 | 0xD0 | Client Info | misc |
| 250 | 0xFA | Full State | misc |

### Ignored (No-op)

```
8, 31, 32, 36, 54, 55, 56, 67, 72, 102, 109,
126, 127, 128, 134, 139, 141, 145, 146, 147,
148, 160, 187
```

## Reference Files

- [STRUCTURES.md](STRUCTURES.md) - Common data structures (VehicleData, ItemData, etc.)
- [CMD_MAP.md](CMD_MAP.md) - Complete CMD → Handler mapping
- [messages.md](messages.md) - Error message reference

## See Also

- [PROTOCOL.md](../PROTOCOL.md) - Main protocol overview

