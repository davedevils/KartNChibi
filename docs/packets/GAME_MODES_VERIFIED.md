# Game Modes Verified

Ground truth for KnC game mode system. Reversed from client. Byte and switch order checked against the loaded binary.

## Source of truth

| Fact | Value |
|------|-------|
| Authoritative dump | `DevClient/KnC-new.exe.c` full Hex-Rays of the loaded build |
| Cross check | `docs/packets/PACKET_REGISTRY.md` |
| Do NOT trust for addr | `DevClient/KnC-ghydra.exe.c` `DevClient/extracted/*.c` partial dumps |
| Dispatcher | `sub_4777C0` switch on u16 opcode |
| Read prim | `sub_44E910(pkt,dst,N)` read N bytes `sub_44EB60` wstring `sub_44EB30` ascii |

## Two mode fields

Client has two separate mode ints do not conflate.

| Field | Addr | Set by | Role |
|-------|------|--------|------|
| room type mode | `dword_BCE210` | 0x13 room data `sub_47FC20` | governs players team rules banners results |
| setup mode | `dword_B2316C` | 0x14 GAME_MODE_SETUP `sub_479CC0` | race start trigger values 3 and 8 only |

room type `dword_BCE210` is the main mode enum. setup mode `dword_B2316C` is a distinct value read at race start meaning of 3 vs 8 unresolved.

## Mode ID table room type dword_BCE210

Values 0..4. Same order in create room buttons `sub_4631F0` waiting room banners `sub_410CB0` load and draw index `sub_410640`.

| ID | Create btn | Banner png | Meaning | Max players | Team | Min start |
|----|-----------|-----------|---------|-------------|------|-----------|
| 0 | CreatRoom_I_S | WaitingRoom_Top_ItemSingle | Item Race solo | 8 | no | 2 |
| 1 | CreatRoom_I_T | WaitingRoom_Top_ItemTeam | Item Race team | 8 | yes | 4 balanced |
| 2 | CreatRoom_S_S | WaitingRoom_Top_SpeedSigle | Speed Race solo | 16 | no | 2 |
| 3 | CreatRoom_S_T | WaitingRoom_Top_SpeedTeam | Speed Race team | 16 | yes | 4 balanced |
| 4 | CreatRoom_B | WaitingRoom_Top_Battle | Battle deathmatch | 8 | no | 2 |

button strings `sub_4631F0` lines 208207-208211. banner load `sub_410CB0` lines 145588-145592 slots 72544 72696 72848 73000 73152 step 152. draw index at struct off 285712 `sub_410640` line 145332 `152 * idx + 72544`. battle special idx 4 `sub_410640` line 145751.

Item modes cap 8. Speed modes cap 16. Battle caps 8. proven by create room max user set `sub_4634C0` lines 208315-208322 mode 2 or 3 sets 16 else 8. and start check `sub_40C950` case table below.

## Start race rules per mode

`sub_40C950` line 142551 switch on `dword_BCE210`. members `dword_BCE230` capacity `dword_BCE22C` red count `dword_BCE238` blue count `dword_BCE23C`.

| Check | Modes | Rule | Fail msg | Line |
|-------|-------|------|----------|------|
| alone guard | 0 2 4 | members >= 2 | MSG_NOT_ALONE | 142533 |
| team balance | 1 3 | red == blue | MSG_NOT_BALLENCE_START | 142540 |
| team min | 1 3 | members >= 4 | MSG_SMALL_MEMBER_ERROR | 142545 |
| cap 8 | 0 1 4 | members <= 8 and cap <= 8 | MSG_MAX_ROOM_USER_8 | 142566 |
| cap 16 | 2 3 | members <= 16 and cap <= 16 | MSG_MAX_ROOM_USER_16 | 142574 |

lighter dup check `sub_40CB00` line 142605 same cap table no balance no min.

## Create room flow

| Step | Fn | Note |
|------|----|------|
| popup load 5 mode btns | `sub_4631F0` | I_S I_T S_S S_T B order = mode 0..4 |
| mode select set max user | `sub_4634C0` | read sel mode off 26432 if 2 or 3 max 16 else 8 |
| send | `sub_480CC0` | build payload then `sub_476B80` |

send payload `sub_480CC0` lines 225416-225421.

| # | Type | Src | Note |
|---|------|-----|------|
| 1 | wstring | String | room name |
| 2 | wstring | a3 | password |
| 3 | int32 | a4 = off 26436 | max users 8 or 16 |
| 4 | int32 | a5 = off 26432 | mode 0..4 |
| 5 | int32 | a6 = off 26428 | flag |
| 6 | int32 | a7 | public 0 private 1 |

C to S opcode was read as C_CREATE_ROOM 0x63 by the old command map, the registry says the client sends 0x2D and 0x63 is the ack. tag arg 45 in `sub_44ECD0` is a buffer class not the opcode.

## Lobby room list icons

`sub_408020` loads per mode room list icons line 139317+.

| Png | Mode |
|-----|------|
| Lobby_Room_I_Single | 0 Item solo |
| Lobby_Room_I_Team | 1 Item team |
| Lobby_Room_S_Single | 2 Speed solo |
| Lobby_Room_S_Team | 3 Speed team |
| Lobby_Room_B | 4 Battle |
| Lobby_Room_Speed Lobby_Room_Item | generic tab icons |

quick match buttons `Lobby_Quick_I_Single` id 0 `Lobby_Quick_I_Team` id 1 `Lobby_Quick_S_Single` id 2 `Lobby_Quick_S_Team` id 3 lines 139958-139961 same 0..3 order.

## In race per mode differences

| Feature | Fn | Behavior |
|---------|----|----------|
| nameplate color | `sub_4990A0` line 241612 | modes 0 2 4 solo color by self else team red blue via `dword_BA17EC` modes 1 3 |
| track select reset | `sub_474A30` line 217493 | mode != 4 resets track idx 0 mode 4 keeps own arena set |
| scoreboard rows | `sub_4744F0` line 217123 | mode 4 caps list to 1 row else full |

team flag per player `dword_BA17EC[756*i]` driver id `dword_BA17F4[756*i]` local driver `dword_1A20658`. race running state `dword_B2360C == 9`.

## Results per mode

result screen loads panels `sub_4B72B0` lines 262085-262091.

| Panel png | Used by |
|-----------|---------|
| Result_Item | Item solo mode 0 |
| Result_Speed | Speed solo mode 2 |
| Result_Team_r Result_Team_b | team modes 1 3 red blue |
| Result_Winner Result_Loser | battle when `byte_B23182 == 1` |

`byte_B23182` is the battle result flag drives Winner Loser panel and Winner Loser nameplate `sub_4990A0` line 241642. setter not located in dump.

per player race end 0x3C `sub_47A5C0` 17 bytes lines 220895-220900.

| # | Type | Bytes | Addr |
|---|------|-------|------|
| 1 | int32 | 4 | player id |
| 2 | int32 | 4 | dword_80E664 |
| 3 | int8 | 1 | byte_80E659 |
| 4 | int32 | 4 | dword_80E65C |
| 5 | int32 | 4 | dword_80E1B0 win flag |

win flag nonzero plays win sound `dword_B2318C` zero plays lose sound `dword_B23188` `sub_47A5C0` lines 220910-220913.

results list 0x3A `sub_47AE00` reads one int32 per player detail via `sub_47AE30` see PACKET_REGISTRY.md.

## 0x14 GAME_MODE_SETUP setup mode

`sub_479CC0` guarded by `dword_F727F4 != 2`.

| Field | Type | Cond | Addr |
|-------|------|------|------|
| mode | int32 | always | dword_B2316C |
| extra1 | int32 | mode 3 or 8 | unk_B23170 |
| extra2 | int32 | mode 3 or 8 | dword_B23174 |
| extra3 | int32 | mode 3 or 8 | dword_B23178 |
| extra4 | int32 | mode 3 or 8 | dword_B23198 |
| extra5 | int32 | mode 3 or 8 | dword_B2319C |

when mode 3 or 8 client calls `sub_4538B0` then `sub_404410(dword_B23288, 11)` enter game stage 11. line 220567. mode 3 or 8 meaning vs room type enum unresolved likely a separate match category.

## Single player game modes

driven by UI stage switch `sub_404410` not by room type. each case inits a stage object.

| Stage | Case | Init fn | Obj | String |
|-------|------|---------|-----|--------|
| game race MP | 11 | sub_4028C0 | dword_B0C8A8 | Stage game |
| tutorial game | 13 | sub_41B860 | qword_BFC3A0 | Stage tutorial |
| TutorialMenu | 14 | sub_437190 | unk_CDA680 | Stage TutorialMenu |
| GhostMode game | 15 | sub_425CB0 | unk_C1A928 | Stage GhostMode game |
| Quest game | 17 | sub_42D690 | dword_C70860 | Stage Quest game |
| CarFactory | 18 | sub_430EF0 | unk_C70AA8 | Stage CarFactory |
| RoomEditer | 19 | sub_434C00 | unk_C96D08 | Stage RoomEditer |
| ScenarioMenu | 22 | sub_437FB0 | unk_CFC820 | Stage ScenarioMenu |
| GhostMode menu | 23 | sub_439350 | unk_D02A48 | Stage GhostMode |
| MissionMenu | 24 | sub_43BAD0 | unk_D09EB0 | Stage MissionMenu |
| Mission game | 25 | sub_43A320 | dword_D09558 | Stage Mission game |
| QuestMenu | 26 | sub_43CFC0 | unk_D12218 | Stage QuestMenu |

license mode has no own stage case runs on tutorial game stage with replay reps `License_Track_02.rep` `_03` `_11` `_12`. license flow via C to S 0xAB test 0xAC result. pass fail strings `MSG_LICENSE_10..12` `License/judge/result_success` `result_fail`.

pass fail strings per single player mode.

| Mode | Success | Fail |
|------|---------|------|
| tutorial | MSG_TUTORIAL_SUCC | MSG_TUTORIAL_FAIL |
| ghost | MSG_GHOST_MODE_S | MSG_GHOST_MODE_F |
| license | MSG_LICENSE_10..12 | License/judge/result_fail |

## C to S mode entry opcodes

from the old command map client to server, the registry is the reference.

| Opcode | Name | Mode |
|--------|------|------|
| 0x63 | C_CREATE_ROOM | make room mode 0..4 |
| 0x40 | C_GAME_START | start MP race |
| 0xA9 | C_START_TUTORIAL | tutorial |
| 0xAA | C_TUTORIAL_COMPLETE | tutorial |
| 0xAB | C_LICENSE_TEST | license |
| 0xAC | C_LICENSE_RESULT | license 3xint32 |
| 0xC0 | C_GHOST_MENU | ghost |
| 0xC1-C6 | Ghost ops | ghost |
| 0xC7-CD | Scenario ops | scenario quest |

## Packets that differ per mode

| Opcode | Fn | Mode effect |
|--------|----|-------------|
| 0x13 | sub_47FC20 | carries room type mode field 4th int32 |
| 0x14 | sub_479CC0 | setup mode 3 or 8 adds 5 int32 |
| 0x3C | sub_47A5C0 | last int32 win flag drives win lose sound |
| 0x21 3E | sub_4797C0 sub_479D60 | seat has team flag used only modes 1 3 |
| 0x23 | sub_479BE0 | player update packs ready team relevant team modes |

## Open unknowns

| Item | Why |
|------|-----|
| 0x14 setup mode 3 vs 8 | distinct enum from room type not mapped |
| off 285712 banner idx write | draw reads it setter not found assumed mirror of dword_BCE210 |
| byte_B23182 battle flag setter | read many places write not in dump |
| dword_BCE22C vs BCE230 | capacity vs current member count both cap checked |
| create room off 26428 flag | 5th int32 purpose unknown |
| 0x13 fields BCE214 218 21C 220 224 244 | 6 int32 room settings unnamed |
