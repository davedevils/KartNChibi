# KnC Protocol Documentation

Reverse engineered from KnC.exe (Chibi Kart / Kart n' Crazy)

---

## Packet Structure

```
┌──────────────────────────────────────────────────────────┐
│ HEADER (8 bytes)                                         │
├──────────────────────────────────────────────────────────┤
│ [0-1]  Size     uint16_t  Payload size (little-endian)   │
│ [2]    CMD      uint8_t   Command/Opcode                 │
│ [3]    Flag     uint8_t   Sub-command / variant          │
│ [4-7]  Reserved 4 bytes   Usually 0x00                   │
├──────────────────────────────────────────────────────────┤
│ PAYLOAD (variable)                                       │
└──────────────────────────────────────────────────────────┘
```

**Flag byte [3]** distinguishes packet variants:
- `0x00` = Default behavior  
- `0x01` = UI transition / alternate mode

**Total packet size** = Header (8) + Payload (Size field value - 4)

---

## Data Types

| Type | Size | Description |
|------|------|-------------|
| `int8` | 1 | Signed 8-bit |
| `uint8` | 1 | Unsigned 8-bit |
| `int16` | 2 | Signed 16-bit LE |
| `uint16` | 2 | Unsigned 16-bit LE |
| `int32` | 4 | Signed 32-bit LE |
| `uint32` | 4 | Unsigned 32-bit LE |
| `string` | N+1 | Null-terminated ASCII |
| `wstring` | 2N+2 | Null-terminated UTF-16LE |

---

## Packet Index

### Server → Client

| Category | Packets | Description |
|----------|---------|-------------|
| [Auth](packets/auth/) | 0x01, 0x12, 0x8E-0x90 | Login, heartbeat resp, init |
| [UI](packets/ui/) | 0x0E-0x12 (flag=0x01) | Screen transitions |
| [Inventory](packets/inventory/) | 0x1B-0x1E, 0x78-0x7A | Items, vehicles |
| [Room](packets/room/) | 0x21-0x23, 0x30, 0x3E-0x3F, 0x62-0x63 | Room management |
| [Chat](packets/chat/) | 0x2D, 0x2E | Messaging |
| [Game](packets/game/) | 0x14, 0x31-0x3C | Race, items, results |
| [Shop](packets/shop/) | 0x6F, 0x72-0x74 | Shop transactions |

### Client → Server

| Category | Packets | Description |
|----------|---------|-------------|
| [Client](packets/client/) | 0x07, 0x19, 0x2C, 0x32, 0x4D, 0x73, 0xA6, 0xD0, 0xFA | Auth, heartbeat, state |

---

## Game States

| Value | State | Description |
|-------|-------|-------------|
| 0 | DISCONNECTED | Not connected |
| 1 | CONNECTING | Connection in progress |
| 2 | CONNECTED | Connected, waiting |
| 3 | AUTHENTICATING | Login in progress |
| 4 | AUTHENTICATED | Login success |
| 5 | MENU | Main menu |
| 6 | GARAGE | Garage screen |
| 7 | SHOP | Shop screen |
| 8 | LOBBY | Game lobby |
| 9 | ROOM | In a room |
| 10 | LOADING | Loading race |
| 11 | RACING | In race |
| 12 | RESULTS | Race results |
| 13 | TUTORIAL | Tutorial mode |

---

## Common Structures

See [STRUCTURES.md](packets/STRUCTURES.md) for full details.

| Structure | Size | Used By |
|-----------|------|---------|
| PlayerInfo | 0x4C8 (1224) | CMD 0x07 |
| VehicleData | 0x2C (44) | CMD 0x1B, 0x3E, 0x78 |
| ItemData | 0x38 (56) | CMD 0x1C, 0x3E |
| AccessoryData | 0x1C (28) | CMD 0x1D, 0x1E |
| SmallItem | 0x20 (32) | CMD 0x79, 0x7A |
| ChatMessage | ~116 | CMD 0x2D |

See [CMD_MAP.md](packets/CMD_MAP.md) for complete handler mapping.

---

## Error Messages

| Message | Category | Description |
|---------|----------|-------------|
| `MSG_SERVER_NOT_READY` | Auth | Server starting |
| `MSG_DB_ACCESS_FAIL` | Auth | Database error |
| `MSG_REINPUT_IDPASS` | Auth | Wrong credentials |
| `MSG_INVALID_ID` | Auth | User not found |
| `MSG_DURABILITY_ZERO` | Game | Item broken |
| `MSG_DURABILITY_LOW` | Game | Low durability |
| `MSG_MAX_ROOM_USER_8/16` | Room | Room full |
| `MSG_TEAM_CHANGE_FAIL` | Room | Cannot change team |
| `MSG_UNKNOWN_ERROR` | General | Unknown error |

See [messages.md](packets/messages.md) for complete list.

---

## Notes

- **CMD Offset**: Client uses -1 internally, server sends actual values
- **Rate Limit**: Heartbeat (0xA6) = 1000ms minimum
- **Timeout**: Connection = 5500ms (0x157C)
- **Strings**: Chat = UTF-16LE, Error messages = ASCII
