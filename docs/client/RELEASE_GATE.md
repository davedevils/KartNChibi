# Release gate

Ran 2026-09-24 against `knc-server:20260924b` (knc-game, knc-login, knc-mariadb checked healthy first).

Run with `python tools/release_gate.py`, options `--only <names>`, `--keep-going`, `--no-fixtures`. A `--only` run merges its rows into the table, it does not drop the rows of scripts it left alone.

| Script | Result | Failing expect | Server warnings | Shots |
|---|---|---|---|---|
| Rerun | Host | race_items ready | weather ready | Scene loaded |
| block | PASS | - | 3 | `reference\gate\2026-09-24\block` |
| buy | PASS | - | 0 | `reference\gate\2026-09-24\buy` |
| carcraft | PASS | - | 0 | `reference\gate\2026-09-24\carcraft` |
| carcraft_factory | PASS | - | 0 | `reference\gate\2026-09-24\carcraft_factory` |
| carcraft_slots | PASS | - | 0 | `reference\gate\2026-09-24\carcraft_slots` |
| channel | PASS | - | 0 | `reference\gate\2026-09-24\channel` |
| create | PASS | - | 0 | `reference\gate\2026-09-24\create` |
| create_driver | PASS | - | 1 | `reference\gate\2026-09-24\create_driver` |
| create_private | PASS | - | 0 | `reference\gate\2026-09-24\create_private` |
| delete | PASS | - | 0 | `reference\gate\2026-09-24\delete` |
| equip | PASS | - | 0 | `reference\gate\2026-09-24\equip` |
| escape | PASS | - | 0 | `reference\gate\2026-09-24\escape` |
| frame_buttons | PASS | - | 0 | `reference\gate\2026-09-24\frame_buttons` |
| gacha | PASS | - | 0 | `reference\gate\2026-09-24\gacha` |
| gacha_close | PASS | - | 0 | `reference\gate\2026-09-24\gacha_close` |
| gear | PASS | - | 0 | `reference\gate\2026-09-24\gear` |
| ghost | PASS | - | 0 | `reference\gate\2026-09-24\ghost` |
| gift | PASS | - | 0 | `reference\gate\2026-09-24\gift` |
| intro | PASS | - | 0 | `reference\gate\2026-09-24\intro` |
| invite_lobby_sample | PASS | - | 0 | `reference\gate\2026-09-24\invite_lobby_sample` |
| licence_grade | PASS | - | 0 | `reference\gate\2026-09-24\licence_grade` |
| licence_pick | PASS | - | 1 | `reference\gate\2026-09-24\licence_pick` |
| licence_test | PASS | - | 0 | `reference\gate\2026-09-24\licence_test` |
| login | PASS | - | 0 | `reference\gate\2026-09-24\login` |
| mission_kind0 | PASS | - | 0 | `reference\gate\2026-09-24\mission_kind0` |
| mission_kind1 | PASS | - | 0 | `reference\gate\2026-09-24\mission_kind1` |
| mission_kind1_sample | PASS | - | 0 | `reference\gate\2026-09-24\mission_kind1_sample` |
| mission_kind2_sample | PASS | - | 0 | `reference\gate\2026-09-24\mission_kind2_sample` |
| missions_menu | PASS | - | 0 | `reference\gate\2026-09-24\missions_menu` |
| options | PASS | - | 0 | `reference\gate\2026-09-24\options` |
| options_default | PASS | - | 0 | `reference\gate\2026-09-24\options_default` |
| options_graphic | PASS | - | 0 | `reference\gate\2026-09-24\options_graphic` |
| options_keys_race | PASS | - | 1 | `reference\gate\2026-09-24\options_keys_race` |
| paint | PASS | - | 0 | `reference\gate\2026-09-24\paint` |
| parts | PASS | - | 0 | `reference\gate\2026-09-24\parts` |
| pendant | PASS | - | 0 | `reference\gate\2026-09-24\pendant` |
| pets | PASS | - | 0 | `reference\gate\2026-09-24\pets` |
| quest | PASS | - | 0 | `reference\gate\2026-09-24\quest` |
| race_items | PASS | - | 1 | `reference\gate\2026-09-24\race_items` |
| repair | PASS | - | 0 | `reference\gate\2026-09-24\repair` |
| room_drive | PASS | - | 0 | `reference\gate\2026-09-24\room_drive` |
| roomcraft | PASS | - | 0 | `reference\gate\2026-09-24\roomcraft` |
| roomcraft_markers | PASS | - | 0 | `reference\gate\2026-09-24\roomcraft_markers` |
| shop_noprice | PASS | - | 0 | `reference\gate\2026-09-24\shop_noprice` |
| shop_tabs | PASS | - | 0 | `reference\gate\2026-09-24\shop_tabs` |
| team | PASS | - | 0 | `reference\gate\2026-09-24\team` |
| tutorial1 | PASS | - | 0 | `reference\gate\2026-09-24\tutorial1` |
| userinfo | PASS | - | 1 | `reference\gate\2026-09-24\userinfo` |
| weather | PASS | - | 1 | `reference\gate\2026-09-24\weather` |

## Real bugs

The first run of 2026-09-23 found three real gaps behind passing scripts. All three are fixed in the
code of the same day. The rows of `race_items`, `weather`, `userinfo` and `invite_lobby_sample` above
are the reruns with the new client against the same package image, the server side of the fixes
needs the next image and is proven in process by `knc-tests` (188 green,
`tests/server/test_race_start_and_invite.cpp`).

### Fixed: the race screen took about 27 s to enter, past the server 3 s window

Before, first run: `screen race enter 28761 ms` (race_items) and `26187 ms` (weather), the window
frozen on the room the whole time, then `[WARN][RACE] room 4 starts with 1 racer(s) that never said
scene loaded` at 07:24:06.263 and the same for rooms 8 and 7 at 07:35:16.629 and 07:35:21.629.

Cause: one thing. The track loader pointed the texture cache at a second `PakReader` of its own,
which byte scanned the whole 867 MB pak the first time a texture was not on disk (the wheel dust
`GRASS.dds`, a board of another theme). With the step lines of the new client the upload read 54
textures in 22971 ms of a 23186 ms entry. It moved with the file cache: 24552 ms and 43125 ms on a
first run, 1502 ms and 1335 ms with the pak hot, which is why weather read the same 27 s after
race_items.

Fix, client: the texture cache reads the pak index the app loaded at the start
(`AssetStore::readPakByName`), the track loads on a thread while the room frame stays up (the stock
holds its last room frame the same way, it has no loading stage or art between the room and the
race, docs/client/README.md Race entry), the textures decode on the spare cores, the frames of the
load are held and replayed in order, the kart and driver parse is cached, the effect nifs parse
after the race stands. Every S2C `0x000D` gets its C2S `0x000D` once the track stands, as the stock
`FUN_00402210` answers it.

After, three reruns of the new client, `screen race enter 1 ms` in all of them:

| Rerun | Host | race_items ready | weather ready | Scene loaded |
|---|---|---|---|---|
| 10:39 | quiet | 435 ms | 233 ms | 93 and 59 ms after the grid, GO at once |
| 12:28, the rows above | 76 to 94 percent CPU, a slow disk | 6276 ms | 6225 ms | inside the 3 s window of the package image, GO at once |

On the busy host the time is file reads (`track nif 10735 ms`, `54 files read 3027 ms` in a sample run
of the same hour, 118 ms on a quiet host), the window stays on the room frame and never freezes. No
rerun has a "never said scene loaded" line. The sample race reads 290 to 465 ms on Toy Circuit and
Pinky Road on a quiet host.

Fix, server (next image): the GO waits for every seated human's `0x000D` up to 30 s after the grid
(`SceneLoadWait`, was 3 s), a racer who leaves before the GO owes nothing. The stock answers
`0x000D` online too, the page said "dead online" by mistake: the S2C handler `FUN_0047ADE0` writes
the flag through its absolute address `0xB23150` (docs/packets/opcodes/0x000D.md). The stock under
wine answered 2870 ms after the grid on the package server.

New line, the rerun only: `[WARN][GAME] rate limit drop opcode 0x000D`. Our client now answers the
second S2C `0x000D` a frame after the first as the stock does, inside the 50 ms default floor of the
limiter. The late answer comes after the GO and is ignored, the next image gives `0x000D` no floor.

### Fixed: the client's own motion stream went stale and briefly out of order

Before, first run: 2 "motion sample out of order from char 9" and 5 "motion stale player 9" in the
race_items window, 2 and 1 in the weather window.

Causes: the frames of the race stalled past 500 ms (the load, a PNG encoded inside the frame at each
capture, the effect nifs parsed and their folders walked on the main thread), the send clock was the
frame clock so a slow frame before the send point put two reports 5 to 67 ms apart on the wire, the
socket had no TCP NODELAY, and the server named two reports of one read "out of order".

Fix: `0x0040` leaves at most once a frame once 100 ms of the real clock passed, the rule of the
stock `motion_send_0x40` 0x49BEB0, in the race and in the room drive. The socket sets TCP NODELAY as
the stock `sub_4769C0`. Captures encode on their own thread, the loader threads run below normal
priority. The server logs a same ms pair at debug as "bunched in one read" (next image).

After: no "out of order" line in any rerun, the smallest report gap is 99 to 101 ms in every wire log.
The quiet rerun of 10:39 has no stale line in either window. The busy reruns still show a few
(race_items 10:18 on a host at 88 to 97 percent CPU, weather 12:30 two lines): their `[time] frame`
lines put the time in the frame pacing sleep, the event poll or the draw (`frame 500 ms ... sleep 494`,
`frame 551 ms ... draw 551`), the thread waits for a core, the stock would stall the same. The one frame
of a second at the race ready is the main thread part of the load (`update 1310`), before the GO.

### Fixed: a room invite sent from the lobby was dropped silently

Before: `[WARN][SOCIAL] Room invite from char 9 but sender is not in a room`, no answer on the wire.

The stock sends it: the Invite item of the messenger row menu is id 0xD of `FUN_00467A70`, the jump
table at `0x4689E0` goes to `0x46862B` and `sub_481CD0` sends `0x006C` with the friend name, no room
check, no grey state. So the item stays as it is in our client and the server answers.

Fix, server (next image): every refusal of `doRoomInvite` answers on `0x0126`, a lobby invite gets
`MSG_UNSUPPORT` (Not available). Fix, client: the `0x0126` line prints the resolved key alone as
`sub_47EC00` does. Proof: `invite_lobby_sample` on the sample server, which answers the same way, the
lobby chat shows Not available (`invite_refused_lobby.png`), and the tests. `userinfo` on the package
image still logs the drop until the new image.
