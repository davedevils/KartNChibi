# Room Packets

Room management and player interactions.

## Server → Client

| CMD | Name | Description |
|-----|------|-------------|
| [0x21](0x21_ROOM_FULL.md) | Room Full | Room is full notification |
| [0x22](0x22_LEAVE_ROOM.md) | Leave Room | Player left room |
| [0x23](0x23_PLAYER_UPDATE.md) | Player Update | Update player slot |
| [0x25](0x25_ROOM_STRING.md) | Room String | Room data (~288 bytes) |
| [0x27](0x27_ROOM_INFO_ALT.md) | Room Info Alt | Alt format (~288 bytes) |
| [0x28](0x28_ENTITY_DATA.md) | Entity Data | Entity (104 bytes) |
| [0x30](0x30_ROOM_STATE.md) | Room State | Update room state |
| [0x3E](0x3E_PLAYER_JOIN.md) | Player Join | Player joined room |
| [0x3F](0x3F_ROOM_INFO.md) | Room Info | Full room data |
| 0x5C | Room Data | 3×int32 (12 bytes) |
| 0x5D | Ready State | Player ready toggle |
| 0x5E | Team Change | Team assignment |
| [0x62](0x62_TUTORIAL_FAIL.md) | Tutorial Fail | Tutorial init error |
| [0x63](0x63_CREATE_ROOM.md) | Create Room | Room creation response |
