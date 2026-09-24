# Lobby, rooms and chat

What the game server does between the channel screen and the race start, plus chat and the
messenger. Wire layouts are on the opcode pages in `docs/packets/opcodes/`, the mode table in
`docs/packets/GAME_MODES_VERIFIED.md`.

## Source files

| Area | File |
|------|------|
| routing of every opcode below | `server/game/src/GameServerDispatch.cpp` |
| channel, lobby list, create, join, leave | `server/game/src/GameServerRoomHandlers.cpp` |
| quick match, kick, ready, team, room screen | `server/game/src/GameServerRaceHandlers.cpp` |
| bots waiting for a start, random invite target | `server/game/src/GameServerBootstrap.cpp` |
| room model, seats, bots, start rule | `shared/src/src/game/Room.cpp` |
| chat, whisper, messenger, notes, operator commands | `server/game/src/handlers/SocialHandler.cpp` |
| room decor tail of 0x13 | `server/game/src/handlers/RoomCraftHandler.cpp` |

## Channel and lobby

| C2S | Server |
|-----|--------|
| `0x18` screen, channel | screen 0x0E sends S2C `0x12` and the room list, anything else S2C `0x11` the menu |
| `0x12` lobby enter | leaves the current room, S2C `0x12`, then the room list |

The room list is one S2C `0x2E` remove then one `0x2D` add per room, dripped under the 8 KB
client buffer, because the client clears its grid on every entry and never checks a row id.
The icon is the room mode 0 to 4, the counter counts humans. The channel id is not used, one
game server is one channel.

## Modes and seats

| Mode | Name | Seats | Teams |
|------|------|-------|-------|
| 0 | item single | 8 | no |
| 1 | item team | 8 | red and blue |
| 2 | speed single | 16 | no |
| 3 | speed team | 16 | red and blue |
| 4 | battle | 8 | no |

`GameMode` in `Room.h` uses these names. `Room` keeps humans by session id and bots in their
own list, one seat allocator serves both so a slot is never handed out twice (the client
drops a second 0x21 on a taken slot). In a team mode a newcomer and a bot take the smaller
side, red on a tie. Bots carry ids from 900000 and never touch the database.

## Room flow

| C2S | Server |
|-----|--------|
| `0x2D` create: name, password, max users, mode, flag, private | caps seats by mode, flags team modes, seats the creator as master, sends the room screen |
| `0x2F` join: room id, password, also `0x6E` from an invite and the old `0x3F` | refuses a missing, running, full or locked room and a character with no owned kart, then the room screen to the joiner and slot plus member to the others |
| `0x35` track select | looks the track up in `track_catalog`, keeps the old one when unknown, broadcasts |
| `0x33` ready or start | a guest toggles ready, echoed to the room, the master starts through `Room::startRefusal` |
| `0x64` in a room | team change, team modes only, a side holds at most half the seats |
| `0x118` padlock | master only, up to 9 ascii characters, sent to the room and to the lobby |
| `0x12F` random invite | master only, asks an idle player with S2C `0x12F`, a CPU car takes the seat when nobody answers in 5 s |
| `0x39` | master kicks a human, bots are ignored |
| `0xD9` loadout swap | owned driver and kart instances only, then S2C member update |
| `0x40` of 8 bytes or more while waiting | the kart on the room field, relayed to the others |
| `0x3B` leave race | race bookkeeping, then the lobby |

The room screen (`sendRoomScreen`) is one write: `0x13` context with the decor tail, thirty
`0x32` slot flags, one `0x21` per member with the receiver last among humans, `0x30` the
master, `0x35` the track. `0x63` and `0x3F` are not part of it, `0x63` opens the create popup
and `0x3F` despawns a racer.

A room with one human waiting gets a CPU car every 20 s up to 3 (`startMatchTick`). A quick
match (`0x2C` kind 0 to 3, or `0x64` outside a room) joins an open public room of that mode or
opens one filled with bots.

## Start rule

`Room::startRefusal` returns the client message key of the first rule that fails, the same
checks `sub_40C950` runs in the stock client. Bots count as members and as a side.

| Rule | Modes | Key |
|------|-------|-----|
| room waiting and not empty | all | `MSG_NOT_AVAILABLE_START` |
| at most 8 members | 0 1 4 | `MSG_MAX_ROOM_USER_8` |
| at most 16 members | 2 3 | `MSG_MAX_ROOM_USER_16` |
| red equals blue | 1 3 | `MSG_NOT_BALLENCE_START` |
| at least 4 members | 1 3 | `MSG_SMALL_MEMBER_ERROR` |
| every guest ready | all | `MSG_NOT_AVAILABLE_START` |

The alone guard of the solo modes (two members) stays with the client so a lone tester can
start. Test `RoomStartRule`.

## Chat

C2S `0xB4` goes through `SocialHandler::handleChatSend`: the operator mute and flood mute, the chat
commands, then a whisper prefix, a team prefix, an open small talk, or a normal line. A normal
line goes to the room when the sender sits in one, else to every player in the lobby.
C2S `0xB5` is the whisper by player id, refused when the target blocked the sender or is
offline.

Chat commands:

| Command | Who |
|---------|-----|
| `/w name text`, `/t text` | everyone |
| `/gm ...` who, announce, kick, mute, unmute, ban, unban, and gold cash level at level 2 | operator accounts |
| `/getmoney n`, `/getastro n`, `/getexp n` | only when `KNC_DEV_COMMANDS=1` or the account is an operator, off by default |

## Messenger

| C2S | Meaning |
|-----|---------|
| `0x6C` `0x6F` `0x7A` `0x7D` by name | room invite, friend add, block add, small talk request |
| `0x70` `0x71` `0x74` `0x7B` `0x7E` `0x7F` `0x80` by id | friend accept, reject, delete, block delete, small talk accept, decline, close |
| `0x6D` | room invite answer |
| `0x72`, `0x0133` | user info by name, by id |
| `0x73` | friend status poll, every 3 s |
| `0x81`, `0x84`, `0x85` | note send, mark read, delete |
| `0x0132` | user list page |
| `0x0130` | client option 11, a non zero value drops every room invite, stored on the account |

## Open items

| Item | Why |
|------|-----|
| channels do not split rooms | one game server per `servers` row |
| `ChatHandler` and the chat and list functions of `LobbyHandler` | no route reaches them, safe to delete |
| C2S `0x22`, `0x23` and `0x32` routes | no client sender on the opcode pages, the handlers only touch the sender room |
