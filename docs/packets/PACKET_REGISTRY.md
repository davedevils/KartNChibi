# PACKET REGISTRY

Authoritative opcode registry for the KnC private server emu and the future client clone.
Source: dispatcher `sub_4777C0` in `DevClient/KnC.exe`, image base `0x400000`, plus the
C2S sender census over every `sub_44ECD0(buf, N)` and `sub_4803A0(this, N)` call site.

Scope: opcode `0x0000` to `0x0135`. `0x0135` is the last dispatcher case. `0x0133` is the
highest C2S sender. Nothing above `0x0135` exists in this build.

## 1. Orientation

### 1.1 Frame format

```
offset  size  field
+0x00   4     unknown header dword
+0x04   2     payload_len u16 little endian   (sub_44E900)
+0x06   2     opcode      i16 little endian   (sub_44E8F0 = **(this+1), SIGNED)
+0x08   n     payload
```

Header is 8 bytes. `sub_477520` accounts every consumed frame as `payload_len + 8`.

### 1.2 The 8192 limit

`sub_477520` line 218597: `if ((u16)sub_44E900(frame) + 8 >= 0x2000) { *(this+3) = -4; break; }`.
A frame of 8192 bytes or more KILLS the connection. Max payload is 8183 bytes.
Every list packet must be chunked under that. Worked caps:

| record | max rows per frame |
|---|---|
| 0x1C replay frame (0xAF, 0xF7) | 292 |
| 0x30 roomcraft row (0x010F) | 170 |
| 0xB0 ghost record (0xAC) | 45 |
| 8 byte pair (0x0C, 0x88) | 1022 |

### 1.3 Opcode reuse per direction

The SAME opcode number is often TWO unrelated messages, one per direction.
`0x2D` C2S is CreateRoom `{wstr, wstr, 4x u32}`, `0x2D` S2C is a room list row add.
`0x0004` C2S is CreateCharacter, `0x0004` S2C is the result.
Never build a reply by echoing the request bytes.

BLIND ECHO IS THE #1 CRASH SOURCE. `GameServer` MIRROR_UI answers some opcodes with a
zero payload echo. Where the S2C handler reads fields, that echo makes the client read
past the frame end and throw `Invalid Read`. Proven bad echoes: `0x25`, `0x33`, `0x35`,
`0x47`, `0x49`, `0x4B`, `0x57`, `0x5C`, `0x5F`, `0x65`, `0x69`, `0x6A`, `0xB5`, `0xCF`,
`0xD9`, `0xED`, `0xF5`, `0xF8`, `0xFE`.

### 1.4 Reader and writer functions

| fn | dir | shape |
|---|---|---|
| `sub_44E910(pkt, dst, n)` | read | raw fixed n bytes |
| `sub_44E9C0(pkt, src, n)` | write | raw fixed n bytes |
| `sub_44EB30(pkt, dst)` | read | ASCII cstr, strlen+1, NO bound on dst |
| `sub_44EAD0(pkt, src)` | write | ASCII cstr, strlen+1 |
| `sub_44EB60(pkt, dst)` | read | UTF16LE wstring, 2*wcslen+2, NO bound on dst |
| `sub_44EB00(pkt, src)` | write | UTF16LE wstring, 2*wcslen+2 |
| `sub_44E7F0` / `sub_44E610` | dec / enc | packed u64 position, 0.01 resolution |
| `sub_44ECD0(buf, opcode)` | write | begin packet |
| `sub_4803A0(this, opcode)` | write | zero payload send |
| `sub_476B80` | send | flush |

ASCII vs UTF16 is NOT guessable from the field name. Mixed encoding packets exist:
`0x0007`, `0x0019`, `0x0025`, `0x0027`, `0x006C`, `0x00A7`, `0x00CE`, `0x00D0`, `0x0126`.
Get the order wrong and every packet behind it in the same TCP read desyncs.

No string on the wire is length prefixed. All strings are NUL terminated.
Destination buffers are fixed and unchecked. Clamp server side or smash the client stack.

### 1.5 Screen ack rule

`sub_484010` snapshots the network containers BEFORE the stage init runs.
So ALL data for a screen must be on the client BEFORE the ack that pushes that screen.
Anything sent after the ack is invisible on that screen.

| ack | stage | must arrive first |
|---|---|---|
| `0x000F` garage | 6 | 0x1B 0x1C 0x1D 0x1E 0xBF 0xC0 0xC1 0xC2 |
| `0x0010` shop | 7 | 0xC6 price rows, 0x103 0x108 0x10C defs |
| `0x0012` lobby | 8 | every 0x2D room row |
| `0x0013` room | 9 | decor list is inside the packet |
| `0x0016` license | 14 | 0xA2 progress, 0xC5 test defs |
| `0x008F` mission menu | 24 | 0x87 defs, 0x88 progress |
| `0x010A` carcraft | 18 | 0x107 presets, 0x108 defs, 0x109 instances |
| `0x010E` roomcraft | 19 | 0x10C defs, 0x10D instances |

`0x000F`, `0x0010`, `0x0012`, `0x010A` call `sub_484010`. `0x0011`, `0x0016`, `0x008F`,
`0x010E` do NOT, but the stage init still snapshots counts so the rule holds anyway.
`0x000F`, `0x0010`, `0x0011`, `0x0012`, `0x008F`, `0x010A`, `0x010E`, `0x0013`, `0x0014`,
`0x011C`, `0x011D` all no op while `dword_F727F4 == 2`, the fatal dialog latch.

### 1.6 Modal rule

Most C2S verbs raise `MSG_WAIT` type 0 before sending. Only a packet that calls
`sub_4538B0` clears it. If the server stays silent the client hangs forever.
`0x0039` is the generic zero payload modal close ack.

Message box type 2 calls `sub_4775C0(net, 0)` which CLOSES THE SOCKET and latches
`dword_F727F4 = 2`. Never use type 2 for a recoverable error.

### 1.7 Status vocabulary

| status | meaning |
|---|---|
| PROVEN | layout read off the client disassembly |
| NULLSUB | reaches `nullsub_1 @0x0046F7A0`, a real arm that does nothing |
| NO CASE | dispatcher default arm, payload never read, silently dropped |
| UNKNOWN | no handler AND no sender found, opcode unassigned in this build |

INFERRED is marked inline on any field or meaning that is not proven.

Legend for the Server column: `*` prefix means a live caller exists in this server tree
today per the module wiring reports. No prefix means the symbol exists but is not wired.
`NONE` means no server symbol at all.

### 1.8 Registry counts

| metric | value |
|---|---|
| opcode numbers covered, 0x0000 to 0x0135 | 310 |
| S2C rows | 279, 7 of them cover a numeric range |
| S2C PROVEN | 191 |
| S2C NULLSUB, real arm that does nothing | 23 |
| S2C NO CASE, dispatcher default | 59 |
| S2C UNKNOWN, unassigned number | 6 rows, 12 numbers |
| C2S rows, every one has a real sender | 111 |
| C2S wire PROVEN | 111 |
| C2S rows whose MEANING is INFERRED or UNKNOWN | 12 |
| S2C PROVEN messages with NO server symbol | 41 |
| C2S messages with NO server symbol | 27 |
| S2C rows with a live wired caller | 38 |
| C2S rows with a live wired handler | 35 |

## 2. S2C table

Generated from `opcodes/`, edit the opcode file.

### 2.1 S2C 0x0000 to 0x007F

| Op | Name | Client fn | Server symbol | Size | System | Status |
|---|---|---|---|---|---|---|
| [0x0000](opcodes/0x0000.md) | no case | none | NONE | n/a | dispatcher | NO CASE |
| [0x0001](opcodes/0x0001.md) | MessageKeyBox | sub_478DA0 | PacketBuilder::loginResponse | strlen+5 | auth | PROVEN |
| [0x0002](opcodes/0x0002.md) | MessageTextBox | sub_478D40 | *ShopPackets::rejectWide | 2*(n+1)+4 | msgbox | PROVEN |
| [0x0003](opcodes/0x0003.md) | OpenCharacterCreatePopup | sub_479220 | PacketBuilder::trigger | 0 | charcreate | PROVEN |
| [0x0004](opcodes/0x0004.md) | CreateCharacterResult | sub_479230 | CharCreatePackets::createCharacterSuccess | 4, or 104+2*(n+1) | charcreate | PROVEN |
| [0x0005](opcodes/0x0005.md) | no case | none | NONE | n/a | dispatcher | NO CASE |
| [0x0006](opcodes/0x0006.md) | no case | none | NONE | n/a | dispatcher | NO CASE |
| [0x0007](opcodes/0x0007.md) | LoginAccept | sub_479020 | CharCreatePackets::loginAccept | 1228 | auth | PROVEN |
| [0x0008](opcodes/0x0008.md) | ignored | nullsub_1 | NONE | 0 | no op | NULLSUB |
| [0x0009](opcodes/0x0009.md) | no case | none | NONE | n/a | dispatcher | NO CASE |
| [0x000A](opcodes/0x000A.md) | ProfileRefresh | sub_47D3B0 | *ProgressionPackets::statsRefresh | 38 | progression | PROVEN |
| [0x000B](opcodes/0x000B.md) | PingRequest | sub_479190 | PacketBuilder::ack | 0 | keepalive | PROVEN |
| [0x000C](opcodes/0x000C.md) | PingTable | sub_4791A0 | PacketBuilder::dataPairs | 4+8*n | room 3D | PROVEN |
| [0x000D](opcodes/0x000D.md) | RankBoardRebuild | sub_47ADE0 | SpawnPackets::rankBoardRebuild | 0 | race | PROVEN |
| [0x000E](opcodes/0x000E.md) | ChannelList | sub_4793F0 | PacketBuilder::channelList | var | channel | PROVEN |
| [0x000F](opcodes/0x000F.md) | GarageStageAck | sub_479520 | PacketBuilder::showGarage | 0 | garage | PROVEN |
| [0x0010](opcodes/0x0010.md) | ShopStageAck | sub_479550 | PacketBuilder::showShop | 0 | shop | PROVEN |
| [0x0011](opcodes/0x0011.md) | MenuStageAck | sub_4793C0 | PacketBuilder::showMenu | 0 | menu | PROVEN |
| [0x0012](opcodes/0x0012.md) | LobbyStageAck | sub_4795A0 | PacketBuilder::showLobby | 0 | lobby | PROVEN |
| [0x0013](opcodes/0x0013.md) | RoomContext plus decor | sub_47FC20 | *RoomCraftPackets::appendDecorTail | 40+2*(n+1)+48*k | room 3D | PROVEN |
| [0x0014](opcodes/0x0014.md) | SceneChange | sub_479CC0 | PacketBuilder::gameMode14 | 4 or 24 | race | PROVEN |
| [0x0015](opcodes/0x0015.md) | no case | none | NONE | n/a | dispatcher | NO CASE |
| [0x0016](opcodes/0x0016.md) | LicenseStageAck | sub_47E8B0 | MissionPackets::licenseScreenAck | 0 | license | PROVEN |
| [0x0017](opcodes/0x0017.md) | no case | none | NONE | n/a | dispatcher | NO CASE |
| [0x0018](opcodes/0x0018.md) | no case, C2S only | none | NONE | n/a | channel | NO CASE |
| [0x0019](opcodes/0x0019.md) | ServerRedirect | sub_479340 | CharCreatePackets::serverRedirect | strlen+9 | redirect | PROVEN |
| [0x001A](opcodes/0x001A.md) | no case | none | NONE | n/a | dispatcher | NO CASE |
| [0x001B](opcodes/0x001B.md) | OwnedCharacterList | sub_478EC0 | InventoryPackets::ownedCharacterList | 4+0x2C*n | inventory | PROVEN |
| [0x001C](opcodes/0x001C.md) | OwnedKartList | sub_478F20 | InventoryPackets::ownedKartList | 4+0x38*n | inventory | PROVEN |
| [0x001D](opcodes/0x001D.md) | OwnedConsumableList | sub_478F80 | InventoryPackets::ownedItemList | 4+0x1C*n | inventory | PROVEN |
| [0x001E](opcodes/0x001E.md) | OwnedPartRecord | sub_478FE0 | *InventoryPackets::ownedPartRecord | 28 | inventory | PROVEN |
| [0x001F](opcodes/0x001F.md) | ignored | nullsub_1 | NONE | 0 | no op | NULLSUB |
| [0x0020](opcodes/0x0020.md) | ignored | nullsub_1 | NONE | 0 | no op | NULLSUB |
| [0x0021](opcodes/0x0021.md) | RoomMemberJoin | sub_4797C0 | PacketBuilder::roomMember | 187+2*(n+1) | room 3D | PROVEN |
| [0x0022](opcodes/0x0022.md) | RoomMemberLeave | sub_479920 | PacketBuilder::playerDisconnect | 4 | room 3D | PROVEN |
| [0x0023](opcodes/0x0023.md) | RoomListRowFieldUpdate | sub_479BE0 | PacketBuilder::lobbyRoomState | 8 | lobby list | PROVEN |
| [0x0024](opcodes/0x0024.md) | ignored | nullsub_1 | NONE | 0 | no op | NULLSUB |
| [0x0025](opcodes/0x0025.md) | DeadDialog37 | sub_479AB0 | *SocialPackets::deadDialog37 | var | dead UI | PROVEN |
| [0x0026](opcodes/0x0026.md) | no case, C2S only | none | NONE | n/a | room 3D | NO CASE |
| [0x0027](opcodes/0x0027.md) | DeadDialog39 | sub_479A30 | *SocialPackets::deadDialog39 | var | dead UI | PROVEN |
| [0x0028](opcodes/0x0028.md) | UserInfoPopup | sub_479B20 | SocialPackets::userInfoPopup | 104 | social | PROVEN |
| [0x0029](opcodes/0x0029.md) | no case, C2S only | none | NONE | n/a | messenger | NO CASE |
| [0x002A](opcodes/0x002A.md) | WhisperPromptOn | sub_479B60 | *SocialPackets::whisperPrompt | 0 | chat | PROVEN |
| [0x002B](opcodes/0x002B.md) | WhisperPromptOff | sub_479BD0 | PacketBuilder::whisperDisable | 0 | chat | PROVEN |
| [0x002C](opcodes/0x002C.md) | no case, C2S only | none | NONE | n/a | lobby | NO CASE |
| [0x002D](opcodes/0x002D.md) | RoomListRowAdd | sub_479630 | PacketBuilder::lobbyRoomAdd | 32+2*(n+1) | lobby list | PROVEN |
| [0x002E](opcodes/0x002E.md) | RoomListRowRemove | sub_479710 | PacketBuilder::playerLeft | 4 | lobby list | PROVEN |
| [0x002F](opcodes/0x002F.md) | no case, C2S only | none | NONE | n/a | lobby list | NO CASE |
| [0x0030](opcodes/0x0030.md) | ReadyReset plus master set | sub_479760 | PacketBuilder::roomState | 4 | room 3D | PROVEN |
| [0x0031](opcodes/0x0031.md) | RoomListRowPairUpdate | sub_479950 | PacketBuilder::position | 12 | lobby list | PROVEN |
| [0x0032](opcodes/0x0032.md) | RoomSlotEnable | sub_479C70 | PacketBuilder::roomSlotEnabled | 8 | room 3D | PROVEN |
| [0x0033](opcodes/0x0033.md) | ReadyStateChange | sub_4799B0 | PacketBuilder::gameState | 8 | room 3D | PROVEN |
| [0x0034](opcodes/0x0034.md) | AllReadyBroadcast | sub_479A10 | PacketBuilder::flag34 | 0 | room 3D | PROVEN |
| [0x0035](opcodes/0x0035.md) | TrackSelectBroadcast | sub_479C20 | PacketBuilder::lapInfo | 8 | room 3D | PROVEN |
| [0x0036](opcodes/0x0036.md) | ignored | nullsub_1 | NONE | 0 | no op | NULLSUB |
| [0x0037](opcodes/0x0037.md) | ignored | nullsub_1 | NONE | 0 | no op | NULLSUB |
| [0x0038](opcodes/0x0038.md) | ignored | nullsub_1 | NONE | 0 | no op | NULLSUB |
| [0x0039](opcodes/0x0039.md) | ModalCloseAck | sub_479CB0 | PacketBuilder::finish | 0 | room 3D | PROVEN |
| [0x003A](opcodes/0x003A.md) | RaceGo | sub_47AE00 | PacketBuilder::results | 4 | race | PROVEN |
| [0x003B](opcodes/0x003B.md) | no case | none | PacketBuilder::countdown | n/a | race | NO CASE |
| [0x003C](opcodes/0x003C.md) | FinishAndReward | sub_47A5C0 | ResultsPackets::finishReward | 17 | results | PROVEN |
| [0x003D](opcodes/0x003D.md) | RankBroadcast | sub_47A6B0 | ResultsPackets::rankBroadcast | 8 | results | PROVEN |
| [0x003E](opcodes/0x003E.md) | GridSpawn | sub_479D60 | SpawnPackets::gridSpawn | 176+2*(n+1) | race | PROVEN |
| [0x003F](opcodes/0x003F.md) | DespawnRacer | sub_47A050 | PacketBuilder::roomInfo | 4 | race | PROVEN |
| [0x0040](opcodes/0x0040.md) | MotionBroadcast | sub_47FD30 | MotionPackets::motionBroadcast | 1+25*n | motion | PROVEN |
| [0x0041](opcodes/0x0041.md) | no case, C2S only | none | NONE | n/a | race progress | NO CASE |
| [0x0042](opcodes/0x0042.md) | CameraMode or ResultBoard | sub_47A0A0 | *SpawnPackets::cameraMode | 4 | race cam | PROVEN |
| [0x0043](opcodes/0x0043.md) | ignored | nullsub_1 | NONE | 0 | no op | NULLSUB |
| [0x0044](opcodes/0x0044.md) | LapBoardAdvance | sub_47A0E0 | SpawnPackets::lapBoardAdvance | 4 | race HUD | PROVEN |
| [0x0045](opcodes/0x0045.md) | StandingsRow | sub_47A560 | ItemPackets::standingsUpdate | 12 | standings | PROVEN |
| [0x0046](opcodes/0x0046.md) | RaceScoreboard | sub_47A760 | ResultsPackets::scoreboard | 8+n*var | results | PROVEN |
| [0x0047](opcodes/0x0047.md) | ItemEffectSpawn | sub_47A110 | *ItemPackets::itemSpawn | 24 | items | PROVEN |
| [0x0048](opcodes/0x0048.md) | ignored | nullsub_1 | NONE | 0 | no op | NULLSUB |
| [0x0049](opcodes/0x0049.md) | ItemGrantBroadcast | sub_47AAD0 | ItemPackets::grantBroadcast | 12 | items | PROVEN |
| [0x004A](opcodes/0x004A.md) | no case | none | NONE | n/a | none | NO CASE |
| [0x004B](opcodes/0x004B.md) | HomingLaunchBroadcast | sub_47A460 | ItemPackets::homingLaunch | 16 | items | PROVEN |
| [0x004C](opcodes/0x004C.md) | no case | none | NONE | n/a | none | NO CASE |
| [0x004D](opcodes/0x004D.md) | ClientInfoProbe | sub_4790C0 | NONE | 0 | anticheat | PROVEN |
| [0x004E](opcodes/0x004E.md) | DelayedAckArm | sub_479160 | PacketBuilder::timestamp | 0 | keepalive | PROVEN |
| [0x004F](opcodes/0x004F.md) | no case | none | NONE | n/a | none | NO CASE |
| [0x0050](opcodes/0x0050.md) | no case | none | NONE | n/a | none | NO CASE |
| [0x0051](opcodes/0x0051.md) | no case | none | NONE | n/a | none | NO CASE |
| [0x0052](opcodes/0x0052.md) | no case | none | NONE | n/a | none | NO CASE |
| [0x0053](opcodes/0x0053.md) | no case | none | NONE | n/a | none | NO CASE |
| [0x0054](opcodes/0x0054.md) | GameServerHandoff | sub_47AA00 | PacketBuilder::serverRedirect | 8+strlen+1 | redirect | PROVEN |
| [0x0055](opcodes/0x0055.md) | no case | none | NONE | n/a | none | NO CASE |
| [0x0056](opcodes/0x0056.md) | no case | none | NONE | n/a | none | NO CASE |
| [0x0057](opcodes/0x0057.md) | LockStateRelay | sub_47AB40 | ItemPackets::lockStateRelay | 12 | items | PROVEN |
| [0x0058](opcodes/0x0058.md) | DriverAnimState | sub_47ABE0 | ResultsPackets::driverAnimState | 5 | race anim | PROVEN |
| [0x0059](opcodes/0x0059.md) | no case | none | NONE | n/a | none | NO CASE |
| [0x005A](opcodes/0x005A.md) | no case | none | NONE | n/a | none | NO CASE |
| [0x005B](opcodes/0x005B.md) | no case | none | NONE | n/a | none | NO CASE |
| [0x005C](opcodes/0x005C.md) | TurtleLaunchBroadcast | sub_47A500 | ItemPackets::turtleLaunch | 12 | items | PROVEN |
| [0x005D](opcodes/0x005D.md) | no case | none | NONE | n/a | none | NO CASE |
| [0x005E](opcodes/0x005E.md) | no case | none | NONE | n/a | none | NO CASE |
| [0x005F](opcodes/0x005F.md) | PetReachedRelay | sub_47EE70 | ItemPackets::petReachedRelay | 8 | items | PROVEN |
| [0x0060](opcodes/0x0060.md) | no case | none | NONE | n/a | none | NO CASE |
| [0x0061](opcodes/0x0061.md) | no case | none | NONE | n/a | none | NO CASE |
| [0x0062](opcodes/0x0062.md) | Stage13LicenseEnter | sub_479580 | CharCreatePackets::stageTutorial | 0 | license | PROVEN |
| [0x0063](opcodes/0x0063.md) | MakeRoomPopupOpen | sub_47AC30 | PacketBuilder::createRoomResponse | 4 | lobby UI | PROVEN |
| [0x0064](opcodes/0x0064.md) | RoomTeamUpdate | sub_47ACD0 | PacketBuilder::roomStatus | 8 | room teams | PROVEN |
| [0x0065](opcodes/0x0065.md) | TeamGaugeAdd | sub_47AD60 | PacketBuilder::speedUpdate | 8 | race team | PROVEN |
| [0x0066](opcodes/0x0066.md) | ignored | nullsub_1 | NONE | 0 | no op | NULLSUB |
| [0x0067](opcodes/0x0067.md) | no case, C2S only | none | NONE | n/a | race progress | NO CASE |
| [0x0068](opcodes/0x0068.md) | HardTeleport | sub_47AE30 | MotionPackets::teleport | 20 | motion | PROVEN |
| [0x0069](opcodes/0x0069.md) | CarEffectBroadcast | sub_47AF00 | *ItemPackets::effectApply | 7 | items | PROVEN |
| [0x006A](opcodes/0x006A.md) | MotionBlock | sub_47AFE0 | *ItemPackets::raceValue | 6 | motion | PROVEN |
| [0x006B](opcodes/0x006B.md) | no case | none | NONE | n/a | none | NO CASE |
| [0x006C](opcodes/0x006C.md) | RoomInvite | sub_47B030 | SocialPackets::roomInvite | var | messenger | PROVEN |
| [0x006D](opcodes/0x006D.md) | ignored | nullsub_1 | NONE | 0 | no op | NULLSUB |
| [0x006E](opcodes/0x006E.md) | ForceJoinInvitedRoom | sub_47B190 | PacketBuilder::shopCall | 0 | messenger | PROVEN |
| [0x006F](opcodes/0x006F.md) | FriendAddResult | sub_47B3C0 | SocialPackets::friendAddResult | 4, 48 or 40 | friends | PROVEN |
| [0x0070](opcodes/0x0070.md) | FriendRequestResolved | sub_47B300 | SocialPackets::friendRequestResolved | 8 | friends | PROVEN |
| [0x0071](opcodes/0x0071.md) | FriendRequestResolvedAlt | sub_47B300 | SocialPackets::friendRequestResolved | 8 | friends | PROVEN |
| [0x0072](opcodes/0x0072.md) | UserInfoBlob | sub_47B550 | SocialPackets::userInfoBlob | 104 | messenger | PROVEN |
| [0x0073](opcodes/0x0073.md) | FriendStatusFull | sub_47B570 | SocialPackets::friendStatusFull | 4+12*n | presence | PROVEN |
| [0x0074](opcodes/0x0074.md) | FriendDelResult | sub_47B620 | SocialPackets::friendDelResult | 8 | friends | PROVEN |
| [0x0075](opcodes/0x0075.md) | no case | none | NONE | n/a | none | NO CASE |
| [0x0076](opcodes/0x0076.md) | FriendList | sub_47B1B0 | SocialPackets::friendList | 4+0x2C*n | friends | PROVEN |
| [0x0077](opcodes/0x0077.md) | FriendStatusPartial | sub_47B340 | *SocialPackets::friendStatusPartial | 4+8*n | presence | PROVEN |
| [0x0078](opcodes/0x0078.md) | FriendRequestList | sub_47B220 | SocialPackets::friendRequestList | 4+0x24*n | friends | PROVEN |
| [0x0079](opcodes/0x0079.md) | BlockList | sub_47B290 | SocialPackets::blockList | 4+0x20*n | blocks | PROVEN |
| [0x007A](opcodes/0x007A.md) | BlockAddResult | sub_47B4F0 | SocialPackets::blockAddResultOk | 4 or 36 | blocks | PROVEN |
| [0x007B](opcodes/0x007B.md) | BlockDelResult | sub_47B670 | SocialPackets::blockDelResult | 8 | blocks | PROVEN |
| [0x007C](opcodes/0x007C.md) | no case | none | NONE | n/a | none | NO CASE |
| [0x007D](opcodes/0x007D.md) | SmallTalkInvite | sub_47B6B0 | SocialPackets::smallTalkInvite | 4+2*(n+1) | smalltalk | PROVEN |
| [0x007E](opcodes/0x007E.md) | ignored | nullsub_1 | NONE | 0 | no op | NULLSUB |
| [0x007F](opcodes/0x007F.md) | ignored | nullsub_1 | NONE | 0 | no op | NULLSUB |

### 2.2 S2C 0x0080 to 0x0135

| Op | Name | Client fn | Server symbol | Size | System | Status |
|---|---|---|---|---|---|---|
| [0x0080](opcodes/0x0080.md) | ignored | nullsub_1 | NONE | 0 | no op | NULLSUB |
| [0x0081](opcodes/0x0081.md) | MessengerResult | sub_47B8A0 | SocialPackets::messengerResult | 4 | messenger | PROVEN |
| [0x0082](opcodes/0x0082.md) | NoteAppend | sub_47B710 | PacketBuilder::extendedData130 | 396 | notes | PROVEN |
| [0x0083](opcodes/0x0083.md) | NoteAppendSorted | sub_47B7D0 | PacketBuilder::extendedData131 | 396 | notes | PROVEN |
| [0x0084](opcodes/0x0084.md) | NoteMarkReadAck | sub_47B8D0 | NONE | 4 | notes | PROVEN |
| [0x0085](opcodes/0x0085.md) | NoteDeleteAck | sub_47B900 | NONE | 4 | notes | PROVEN |
| [0x0086](opcodes/0x0086.md) | ignored | nullsub_1 | NONE | 0 | no op | NULLSUB |
| [0x0087](opcodes/0x0087.md) | MissionDefinition | sub_47DC10 | MissionPackets::missionDefinition | 188 | mission | PROVEN |
| [0x0088](opcodes/0x0088.md) | MissionProgressList | sub_47B930 | *MissionPackets::missionProgressList | 4+8*n | mission | PROVEN |
| [0x0089](opcodes/0x0089.md) | no case | none | NONE | n/a | unused | NO CASE |
| [0x008A](opcodes/0x008A.md) | MissionUnlocked | sub_47B990 | *MissionPackets::missionUnlocked | 8 | mission | PROVEN |
| [0x008B](opcodes/0x008B.md) | ignored | nullsub_1 | NONE | 0 | no op | NULLSUB |
| [0x008C](opcodes/0x008C.md) | MissionComplete | sub_47B9E0 | *MissionPackets::missionCompleteWithReward | 20 plus reward | mission | PROVEN |
| [0x008D](opcodes/0x008D.md) | ignored | nullsub_1 | NONE | 0 | no op | NULLSUB |
| [0x008E](opcodes/0x008E.md) | no case | none | NONE | n/a | mission | NO CASE |
| [0x008F](opcodes/0x008F.md) | MissionMenuAck | sub_47E950 | MissionPackets::missionMenuAck | 0 | mission | PROVEN |
| [0x0090](opcodes/0x0090.md) | MissionStartAck | sub_47DF30 | MissionPackets::missionStartAck | 8 | mission | PROVEN |
| [0x0091](opcodes/0x0091.md) | ignored | nullsub_1 | NONE | 0 | no op | NULLSUB |
| [0x0092](opcodes/0x0092.md) | ignored | nullsub_1 | NONE | 0 | no op | NULLSUB |
| [0x0093](opcodes/0x0093.md) | ignored | nullsub_1 | NONE | 0 | no op | NULLSUB |
| [0x0094](opcodes/0x0094.md) | ignored | nullsub_1 | NONE | 0 | no op | NULLSUB |
| [0x0095](opcodes/0x0095.md) | GiftInboxReceivedList | sub_47BE00 | NONE | 4+212*n | gift | PROVEN |
| [0x0096](opcodes/0x0096.md) | GiftSentList | sub_47BD80 | PacketBuilder::giftInboxList | 4+212*n | gift | PROVEN |
| [0x0097](opcodes/0x0097.md) | GiftInboxReceivedListAlt | sub_47BE00 | PacketBuilder::giftInboxListSent | 4+212*n | gift | PROVEN |
| [0x0098](opcodes/0x0098.md) | GiftSendOk | sub_47BEF0 | ShopPackets::giftOk | 220 | gift | PROVEN |
| [0x0099](opcodes/0x0099.md) | GiftAppendOne | sub_47BE80 | NONE | 212 | gift | PROVEN |
| [0x009A](opcodes/0x009A.md) | GiftClaimAck | sub_47BF80 | NONE | 8 plus record | gift | PROVEN |
| [0x009B](opcodes/0x009B.md) | GiftDeleteAck | sub_47C270 | NONE | 4 | gift | PROVEN |
| [0x009C](opcodes/0x009C.md) | GiftMarkReadAck | sub_47C2A0 | NONE | 4 | gift | PROVEN |
| [0x009D](opcodes/0x009D.md) | OwnedCharacterAppend | sub_47C2D0 | *InventoryPackets::appendCharacter | 44 | inventory | PROVEN |
| [0x009E](opcodes/0x009E.md) | OwnedKartAppend | sub_47C320 | *InventoryPackets::appendKart | 56 | inventory | PROVEN |
| [0x009F](opcodes/0x009F.md) | OwnedPartAppend | sub_47C370 | *InventoryPackets::appendPart | 28 | inventory | PROVEN |
| [0x00A0](opcodes/0x00A0.md) | ignored | nullsub_1 | NONE | 0 | no op | NULLSUB |
| [0x00A1](opcodes/0x00A1.md) | ItemDrumPicked | sub_47C930 | NONE | 4 | race gimmick | PROVEN |
| [0x00A2](opcodes/0x00A2.md) | LicenseProgressList | sub_47C960 | MissionPackets::licenseProgressList | 4+12*n | license | PROVEN |
| [0x00A3](opcodes/0x00A3.md) | LicenseTestResult | sub_47C3C0 | MissionPackets::licenseTestResult | 20 plus tails | license | PROVEN |
| [0x00A4](opcodes/0x00A4.md) | LicenseGradeSet | sub_47C770 | MissionPackets::licenseGradeUp | 1 | license | PROVEN |
| [0x00A5](opcodes/0x00A5.md) | PositionFixBulk | sub_47C830 | MotionPackets::positionFix | 1+12*n | motion | PROVEN |
| [0x00A6](opcodes/0x00A6.md) | no case | none | NONE | n/a | session | NO CASE |
| [0x00A7](opcodes/0x00A7.md) | ReauthAccept | sub_479080 | CharCreatePackets::reauthAccept | 1228 | session | PROVEN |
| [0x00A8](opcodes/0x00A8.md) | no case | none | NONE | n/a | unused | NO CASE |
| [0x00A9](opcodes/0x00A9.md) | no case | none | NONE | n/a | unused | NO CASE |
| [0x00AA](opcodes/0x00AA.md) | GhostSessionStart | sub_47CBA0 | GhostPackets::ghostSession | 112+2*(n+1) | ghost | PROVEN |
| [0x00AB](opcodes/0x00AB.md) | GhostBoardHeader | sub_47C9C0 | GhostPackets::boardHeader | 4 | ghost | PROVEN |
| [0x00AC](opcodes/0x00AC.md) | GhostBoardTrack | sub_47C9F0 | GhostPackets::boardTrack | 184+176*n | ghost | PROVEN |
| [0x00AD](opcodes/0x00AD.md) | GhostBoardOpen | sub_47CAE0 | GhostPackets::boardOpen | 0 | ghost | PROVEN |
| [0x00AE](opcodes/0x00AE.md) | GhostFrameCount | sub_47CB00 | GhostPackets::ghostFrameCount | 4 | ghost | PROVEN |
| [0x00AF](opcodes/0x00AF.md) | GhostFrameChunk | sub_47CB30 | *GhostPackets::ghostFrameChunk | 4+28*n | ghost | PROVEN |
| [0x00B0](opcodes/0x00B0.md) | GhostSubmitResult | sub_47CC20 | GhostPackets::submitResult | 184+176*n | ghost | PROVEN |
| [0x00B1](opcodes/0x00B1.md) | no case | none | NONE | n/a | ghost | NO CASE |
| [0x00B2](opcodes/0x00B2.md) | no case | none | NONE | n/a | ghost | NO CASE |
| [0x00B3](opcodes/0x00B3.md) | no case | none | NONE | n/a | unused | NO CASE |
| [0x00B4](opcodes/0x00B4.md) | ChatBroadcast | sub_47CD60 | *SocialPackets::chatBroadcast | var | chat | PROVEN |
| [0x00B5](opcodes/0x00B5.md) | WhisperDeliver | sub_47CF80 | *SocialPackets::whisperDeliver | 2448+2*(n+1) | whisper | PROVEN |
| [0x00B6](opcodes/0x00B6.md) | SystemNotice | sub_47AC60 | PacketBuilder::displayText | 2*(n+1)+4 | notice | PROVEN |
| [0x00B7](opcodes/0x00B7.md) | BuyOk | sub_484F50 | *ShopPackets::buyOk | 12 plus record | shop | PROVEN |
| [0x00B8](opcodes/0x00B8.md) | DeleteOk | sub_484690 | *InventoryPackets::deleteAck | 8 | shop | PROVEN |
| [0x00B9](opcodes/0x00B9.md) | EquipUseAck | sub_484770 | *InventoryPackets::equipPetAck | 32 to 64 | garage | PROVEN |
| [0x00BA](opcodes/0x00BA.md) | UnequipAck | sub_484B10 | *InventoryPackets::unequipPetAck | 4 or 32 | garage | PROVEN |
| [0x00BB](opcodes/0x00BB.md) | ignored | nullsub_1 | NONE | 0 | no op | NULLSUB |
| [0x00BC](opcodes/0x00BC.md) | ApplyEquipmentSet | sub_484D90 | *InventoryPackets::applyEquipmentSet | 112, or 12+48*n | garage | PROVEN |
| [0x00BD](opcodes/0x00BD.md) | no case | none | NONE | n/a | unused | NO CASE |
| [0x00BE](opcodes/0x00BE.md) | ClearAllCatalogs | sub_478B50 | *ShopPackets::catalogReset | 0 | catalog | PROVEN |
| [0x00BF](opcodes/0x00BF.md) | DriverCatalogEntry | sub_47F390 | PacketBuilder::driverCatalog | var | catalog | PROVEN |
| [0x00C0](opcodes/0x00C0.md) | KartDefinition | sub_47F4F0 | PartStatPackets::kartDef | var | catalog | PROVEN |
| [0x00C1](opcodes/0x00C1.md) | ItemDefinition | sub_47F6B0 | PartStatPackets::itemDef | var | catalog | PROVEN |
| [0x00C2](opcodes/0x00C2.md) | KartPartDefinition | sub_47F800 | PartStatPackets::kartPartDef | var | catalog | PROVEN |
| [0x00C3](opcodes/0x00C3.md) | TrackDefinition | sub_47F990 | NONE | var | catalog | PROVEN |
| [0x00C4](opcodes/0x00C4.md) | ThemeDefinition | sub_478C40 | NONE | var | catalog | PROVEN |
| [0x00C5](opcodes/0x00C5.md) | LicenseTestDefinition | sub_47FAE0 | NONE | var | license | PROVEN |
| [0x00C6](opcodes/0x00C6.md) | PriceRow | sub_478CB0 | *ShopPackets::priceRow | 28 | shop | PROVEN |
| [0x00C7](opcodes/0x00C7.md) | no case | none | NONE | n/a | none | NO CASE |
| [0x00C8](opcodes/0x00C8.md) | no case | none | NONE | n/a | none | NO CASE |
| [0x00C9](opcodes/0x00C9.md) | PlaySoundCue | sub_47CD20 | NONE | 4 | race audio | PROVEN |
| [0x00CA](opcodes/0x00CA.md) | no case | none | NONE | n/a | none | NO CASE |
| [0x00CB](opcodes/0x00CB.md) | no case | none | NONE | n/a | none | NO CASE |
| [0x00CC](opcodes/0x00CC.md) | no case | none | NONE | n/a | none | NO CASE |
| [0x00CD](opcodes/0x00CD.md) | HitEffectApply | sub_47D1C0 | NONE | 8 | race items | PROVEN |
| [0x00CE](opcodes/0x00CE.md) | SystemMessage | sub_47D250 | NONE | var | chat | PROVEN |
| [0x00CF](opcodes/0x00CF.md) | ItemSlotSet | sub_47D4A0 | NONE | 16 | race items | PROVEN |
| [0x00D0](opcodes/0x00D0.md) | AstroBalance | sub_47D520 | *ProgressionPackets::astroBalance | 4 | wallet | PROVEN |
| [0x00D9](opcodes/0x00D9.md) | PlayerAppearanceUpdate | sub_47D540 | NONE | 164 | room and grid | PROVEN |
| [0x00ED](opcodes/0x00ED.md) | GachaResult | sub_47D5E0 | GachaPetPackets::result | 48 to 233 | gacha | PROVEN |
| [0x00EE](opcodes/0x00EE.md) | KartPeriodUpdate | sub_47D880 | NONE | 20 | race garage | PROVEN |
| [0x00EF](opcodes/0x00EF.md) | no case | none | NONE | n/a | none | NO CASE |
| [0x00F0](opcodes/0x00F0.md) | CarLapFlagSet | sub_47D930 | NONE | 4 | race | PROVEN |
| [0x00F1](opcodes/0x00F1.md) | ServerClockSync | sub_47D970 | NONE | 36 | clock | PROVEN |
| [0x00F2](opcodes/0x00F2.md) | no case | none | NONE | n/a | race gimmick | NO CASE |
| [0x00F3](opcodes/0x00F3.md) | ScenarioDefinition | sub_47DAA0 | ScenarioPackets::scenarioDefinition | 156 | scenario | PROVEN |
| [0x00F4](opcodes/0x00F4.md) | ScenarioProgressList | sub_47DB00 | ScenarioPackets::progressList | 4+8*n | scenario | PROVEN |
| [0x00F5](opcodes/0x00F5.md) | ScenarioStart | sub_47DD10 | ScenarioPackets::scenarioStart | 8 | scenario | PROVEN |
| [0x00F6](opcodes/0x00F6.md) | ReplayFrameCountAlt | sub_47DC70 | *GhostPackets::altFrameCount | 4 | ghost | PROVEN |
| [0x00F7](opcodes/0x00F7.md) | ReplayFrameChunkAlt | sub_47DCA0 | *GhostPackets::altFrameChunk | 4+28*n | ghost | PROVEN |
| [0x00F8](opcodes/0x00F8.md) | ScenarioResult reward | sub_47DFA0 | ScenarioPackets::scenarioResult | 13, 21 or plus tail | scenario | PROVEN |
| [0x00F9](opcodes/0x00F9.md) | ScenarioProgressAppend | sub_47E350 | ScenarioPackets::scenarioProgressAppend | 8 | scenario | PROVEN |
| [0x00FA](opcodes/0x00FA.md) | no case | none | NONE | n/a | screen init | NO CASE |
| [0x00FB](opcodes/0x00FB.md) | QuestDefinition | sub_47DB60 | QuestPackets::questDefinition | 112 | quest | PROVEN |
| [0x00FC](opcodes/0x00FC.md) | QuestStateList | sub_47DBB0 | QuestPackets::questStateList | 4+12*n | quest | PROVEN |
| [0x00FD](opcodes/0x00FD.md) | no case | none | NONE | n/a | none | NO CASE |
| [0x00FE](opcodes/0x00FE.md) | QuestAcceptAck | sub_47F270 | QuestPackets::questAcceptAck | 4 | quest | PROVEN |
| [0x00FF](opcodes/0x00FF.md) | no case | none | NONE | n/a | none | NO CASE |
| [0x0100](opcodes/0x0100.md) | QuestDiscardAck | sub_47F2C0 | QuestPackets::questDiscardAck | 4 | quest | PROVEN |
| [0x0101](opcodes/0x0101.md) | QuestComplete | sub_47F2F0 | QuestPackets::questCompleted | 8 | quest | PROVEN |
| [0x0102](opcodes/0x0102.md) | QuestProgress | sub_47F340 | QuestPackets::questProgress | 8 | quest | PROVEN |
| [0x0103](opcodes/0x0103.md) | PetDefinition | sub_4800D0 | GachaPetPackets::petDefinition | var | pet catalog | PROVEN |
| [0x0104](opcodes/0x0104.md) | OwnedPetList | sub_47E3A0 | GachaPetPackets::ownedPetList | 4+0x1C*n | pet inv | PROVEN |
| [0x0105](opcodes/0x0105.md) | no case | none | NONE | n/a | hole | NO CASE |
| [0x0106](opcodes/0x0106.md) | no case | none | NONE | n/a | hole | NO CASE |
| [0x0107](opcodes/0x0107.md) | CarCraftPresetList | sub_47E400 | CustomCarPackets::presetList | 4+0x34*n | carcraft | PROVEN |
| [0x0108](opcodes/0x0108.md) | CarCraftPartDefinition | sub_480210 | *CustomCarPackets::partDef | var | carcraft | PROVEN |
| [0x0109](opcodes/0x0109.md) | CarCraftPartInstance | sub_47E4C0 | CustomCarPackets::partInstance | 132 | carcraft | PROVEN |
| [0x010A](opcodes/0x010A.md) | CarCraftOpenAck | sub_47E500 | CustomCarPackets::openCarCraftAck | 0 | carcraft | PROVEN |
| [0x010B](opcodes/0x010B.md) | CarCraftSaveResult | sub_47E530 | CustomCarPackets::saveResult | 0x30+0x84*n | carcraft | PROVEN |
| [0x010C](opcodes/0x010C.md) | RoomCraftObjectDefinition | sub_47FF70 | *RoomCraftPackets::objectDefinition | var | roomcraft | PROVEN |
| [0x010D](opcodes/0x010D.md) | RoomCraftOwnedInstance | sub_47D9A0 | *RoomCraftPackets::placedObject | 48 | roomcraft | PROVEN |
| [0x010E](opcodes/0x010E.md) | RoomCraftStagePush | sub_47D9D0 | RoomCraftPackets::stagePush | 0 | roomcraft | PROVEN |
| [0x010F](opcodes/0x010F.md) | RoomCraftSaveAck | sub_47DA00 | *RoomCraftPackets::saveAck | 4+0x30*n | roomcraft | PROVEN |
| [0x0110](opcodes/0x0110.md) | no case | none | NONE | n/a | hole | NO CASE |
| [0x0111](opcodes/0x0111.md) | no case | none | NONE | n/a | hole | NO CASE |
| [0x0112](opcodes/0x0112.md) | ShopExtendOk | sub_484EB0 | ShopPackets::extendOk | 12 or 60 | shop | PROVEN |
| [0x0113](opcodes/0x0113.md) | no case | none | NONE | n/a | hole | NO CASE |
| [0x0114](opcodes/0x0114.md) | CarCraftPresetRenameAck | sub_47E620 | CustomCarPackets::presetRenameAck | 4+strlen+1 | carcraft | PROVEN |
| [0x0115](opcodes/0x0115.md) | ItemSetRemainingUses | sub_47E680 | *InventoryPackets::setRemainingUses | 8 | item inv | PROVEN |
| [0x0116](opcodes/0x0116.md) | WaitRoomSideListAdd | sub_47E6D0 | NONE | 5+2*(n+1) | room 3D | PROVEN |
| [0x0117](opcodes/0x0117.md) | WaitRoomSideListRemove | sub_47E750 | NONE | 4 | room 3D | PROVEN |
| [0x0118](opcodes/0x0118.md) | RoomLockState | sub_47E780 | NONE | 12 | lobby list | PROVEN |
| [0x0119](opcodes/0x0119.md) | PendantDefinition | sub_47E800 | PacketBuilder::pendantDefinition | 8 plus 3 cstr | pendant | PROVEN |
| [0x011A](opcodes/0x011A.md) | PendantOwnedAppend | sub_47E880 | PacketBuilder::entitySimple | 8 | pendant | PROVEN |
| [0x011B](opcodes/0x011B.md) | PendantOwnedAppendAlt | sub_47E880 | PacketBuilder::entitySimple | 8 | pendant | PROVEN |
| [0x011C](opcodes/0x011C.md) | Stage22Ack | sub_47E8E0 | ScenarioPackets::menuOpenAck | 0 | stage 22 | PROVEN |
| [0x011D](opcodes/0x011D.md) | Stage23Ack | sub_47E980 | NONE | 0 | stage 23 | PROVEN |
| [0x011E](opcodes/0x011E.md) | ServerRedirectStage11 | sub_47E9B0 | NONE | strlen+5 | redirect | PROVEN |
| [0x011F](opcodes/0x011F.md) | RaceTimerArm | sub_47EA10 | NONE | 4 | race HUD | PROVEN |
| [0x0120](opcodes/0x0120.md) | MissionCheckpointPath | sub_47EA70 | NONE | 4+12*n | mission | PROVEN |
| [0x0121](opcodes/0x0121.md) | no case | none | NONE | n/a | mission | NO CASE |
| [0x0122](opcodes/0x0122.md) | MissionGo | sub_47EB70 | NONE | 0 | mission | PROVEN |
| [0x0123](opcodes/0x0123.md) | PendantEquipAck | sub_47EAE0 | NONE | 4 | pendant | PROVEN |
| [0x0124](opcodes/0x0124.md) | CarCraftPresetRowUpdate | sub_47EB10 | CustomCarPackets::presetRowUpdate | 52 | carcraft | PROVEN |
| [0x0125](opcodes/0x0125.md) | BoostReadyRelay | sub_47EBB0 | NONE | 5 | race boost | PROVEN |
| [0x0126](opcodes/0x0126.md) | SystemChatLine | sub_47EC00 | *SocialPackets::systemChatLine | var | chat | PROVEN |
| [0x012E](opcodes/0x012E.md) | ClientFileCheck | sub_47ED40 | NONE | 4+16*n | anticheat | PROVEN |
| [0x012F](opcodes/0x012F.md) | RoomInvitePopup | sub_47ED90 | NONE | 4 or var | room invite | PROVEN |
| [0x0130](opcodes/0x0130.md) | no case | none | NONE | n/a | options | NO CASE |
| [0x0131](opcodes/0x0131.md) | ItemRollStream | sub_478E80 | NONE | 400 | anticheat RNG | PROVEN |
| [0x0132](opcodes/0x0132.md) | UserListPage | sub_47EE30 | SocialPackets::userListPage | 296 | user list | PROVEN |
| [0x0133](opcodes/0x0133.md) | no case | none | NONE | n/a | social | NO CASE |
| [0x0134](opcodes/0x0134.md) | no case | none | PacketBuilder::gachaTransactionResult | n/a | gacha | NO CASE |
| [0x0135](opcodes/0x0135.md) | RewardPopup or LevelUpPopup | sub_47EEF0 | ProgressionPackets::rewardPopup | 28 plus tail | progression | PROVEN |

## 3. C2S table

Generated from `opcodes/`, edit the opcode file.

### 3.1 C2S 0x0000 to 0x007F

| Op | Name | Client fn | Server symbol | Size | System | Status |
|---|---|---|---|---|---|---|
| [0x0004](opcodes/0x0004.md) | CreateCharacterRequest | sub_4805D0 | CharCreateHandler::handleCreateCharacter | 4+2*(n+1) | charcreate | PROVEN |
| [0x0007](opcodes/0x0007.md) | LoginRequest | sub_480430 | GameServer::handleClientAuth | 8+2 strings | auth | PROVEN |
| [0x000B](opcodes/0x000B.md) | PingReply | sub_4803A0 arg 11 | GameServer case 0x0B swallow | 0 | keepalive | PROVEN |
| [0x000D](opcodes/0x000D.md) | RaceSceneLoaded | sub_4807A0 | RaceHandler::handleSceneLoaded | 0 | race | PROVEN |
| [0x000F](opcodes/0x000F.md) | OpenGarageRequest | sub_4806C0 | GarageHandler::handleOpenGarage | 0 | garage | PROVEN |
| [0x0010](opcodes/0x0010.md) | OpenShopRequest | sub_4806B0 | ShopHandler::handleEnterShop | 0 | shop | PROVEN |
| [0x0012](opcodes/0x0012.md) | OpenLobbyRequest or HostStartRace | sub_4806D0 | NONE, MIRROR_UI echo | 0 | lobby, room 3D | PROVEN |
| [0x0016](opcodes/0x0016.md) | OpenLicenseScreenRequest | sub_483950 | LicenseHandler::handleOpenLicenseScreen | 0 | license | PROVEN |
| [0x0018](opcodes/0x0018.md) | StageRequestWithChannel | sub_4806F0 | *MissionPackets::parseStageRequest | 8 | channel | PROVEN |
| [0x0019](opcodes/0x0019.md) | ReconnectRequest | sub_4817E0 | GameServer::handleServerQuery | strlen+9 | redirect | PROVEN |
| [0x0025](opcodes/0x0025.md) | NamePopupActionA | sub_4807B0 | NONE | 2*(n+1) | messenger | PROVEN wire, INFERRED meaning |
| [0x0026](opcodes/0x0026.md) | RoomMemberInfoRequest | sub_480850 | NONE, DATA_REQ swallow | 4 | room 3D | PROVEN |
| [0x0029](opcodes/0x0029.md) | NamePopupActionB | sub_4808F0 | NONE, DATA_REQ swallow | 2*(n+1) | messenger | PROVEN wire, INFERRED meaning |
| [0x002C](opcodes/0x002C.md) | LobbyMenuSelect | sub_481010 | GameServer::handleStateChange | 8 | lobby | PROVEN |
| [0x002D](opcodes/0x002D.md) | CreateRoomRequest | sub_480CC0 | GameServer::handleCreateRoom | 16+2 strings | lobby list | PROVEN |
| [0x002F](opcodes/0x002F.md) | JoinRoomRequest | sub_480F30 | GameServer::handleWhisper, WRONG handler | 4+2*(n+1) | lobby list | PROVEN |
| [0x0033](opcodes/0x0033.md) | ReadyToggle | sub_480DE0 | NONE, MIRROR_UI echo | 4 | room 3D | PROVEN |
| [0x0035](opcodes/0x0035.md) | TrackSelectRequest | sub_480E80 | NONE, MIRROR_UI echo | 8 | room 3D | PROVEN |
| [0x0039](opcodes/0x0039.md) | RoomMemberHostAction, kick | inline sub_40D9D0 | GameServer::handleRaceFinish, WRONG | 4 | room 3D | PROVEN wire, INFERRED kick |
| [0x003B](opcodes/0x003B.md) | LeaveRace | sub_4810C0 | NONE | 0 | race | PROVEN |
| [0x0040](opcodes/0x0040.md) | SelfMotionReport | sub_4818A0 | MotionPackets::parseSelfReport | 19 or 28 | motion | PROVEN |
| [0x0041](opcodes/0x0041.md) | CheckpointReport | sub_4810D0 | *SpawnPackets::parseCheckpoint | 8 | race progress | PROVEN |
| [0x0047](opcodes/0x0047.md) | ItemUseRequest | sub_481230 | *ItemPackets::parseUse | 20 | items | PROVEN |
| [0x0049](opcodes/0x0049.md) | ItemGrantedReport | sub_481180 | *ItemPackets::parsePickup | 8 | items | PROVEN |
| [0x004B](opcodes/0x004B.md) | HomingLaunchRequest | sub_481320 | RaceHandler::handleHomingLaunch | 12 | items | PROVEN |
| [0x004D](opcodes/0x004D.md) | ClientInfoBlob | sub_4790C0 | GameServer::handleRequestData | 276 | anticheat | PROVEN |
| [0x004E](opcodes/0x004E.md) | DelayedAckFire | sub_485290 | NONE | 0 | keepalive | PROVEN |
| [0x0057](opcodes/0x0057.md) | LockStateReport | sub_481520 | RaceHandler::handleLockState | 12 | items | PROVEN |
| [0x0058](opcodes/0x0058.md) | DriverAnimEcho | sub_4815F0 | ResultsPackets::parseAnimStateEcho | 1 | race anim | PROVEN |
| [0x005C](opcodes/0x005C.md) | TurtleLaunchRequest | sub_481430 | RaceHandler::handleTurtleLaunch | 8 | items | PROVEN |
| [0x005F](opcodes/0x005F.md) | PetReachedReport | sub_483E00 | RaceHandler::handlePetReached | 8 | items | PROVEN |
| [0x0062](opcodes/0x0062.md) | LicenseTestStart | sub_4806E0 | NONE | 0 | license | PROVEN |
| [0x0064](opcodes/0x0064.md) | RoomTeamSelect | sub_4816A0 | GameServer::handleTeamChange | 4 | room teams | PROVEN |
| [0x0065](opcodes/0x0065.md) | TeamGaugeReport | sub_481740 | NONE, MIRROR_UI echo | 4 | race team | PROVEN |
| [0x0067](opcodes/0x0067.md) | RaceProgressScore | sub_4819E0 | *SpawnPackets::parseProgress | 4 | race progress | PROVEN |
| [0x0068](opcodes/0x0068.md) | RespawnResult | sub_481A80 | *SpawnPackets::parseRespawn | 16 | motion | PROVEN |
| [0x0069](opcodes/0x0069.md) | HitReport | sub_481B60 | *ItemPackets::parseHit | 3 | items | PROVEN |
| [0x006A](opcodes/0x006A.md) | RaceValuePush | sub_481C20 | *ItemPackets::parseRaceValue | 2 | motion | PROVEN |
| [0x006C](opcodes/0x006C.md) | RoomInviteByName | sub_481CD0 | SocialPackets::parseNameRequest | 2*(n+1) | messenger | PROVEN |
| [0x006D](opcodes/0x006D.md) | RoomInviteAnswer | sub_481D70 | *SocialPackets::parseRoomInviteAnswer | 8 | messenger | PROVEN |
| [0x006E](opcodes/0x006E.md) | JoinInvitedRoom | sub_481E20 | NONE | 4+2*(n+1) | messenger | PROVEN |
| [0x006F](opcodes/0x006F.md) | FriendAddByName | sub_481F00 | SocialPackets::parseNameRequest | 2*(n+1) | friends | PROVEN |
| [0x0070](opcodes/0x0070.md) | FriendReqAccept | sub_4820F0 | SocialPackets::parseIdRequest | 4 | friends | PROVEN wire, INFERRED accept |
| [0x0071](opcodes/0x0071.md) | FriendReqReject | sub_482050 | SocialPackets::parseIdRequest | 4 | friends | PROVEN wire, INFERRED reject |
| [0x0072](opcodes/0x0072.md) | UserInfoReqByName | sub_4822D0 | SocialPackets::parseUserInfoReqByName | 2*(n+1) | messenger | PROVEN |
| [0x0073](opcodes/0x0073.md) | FriendStatusPoll | sub_4803A0 arg 115 | *SocialPackets::parseFriendStatusPoll | 0 | presence | PROVEN |
| [0x0074](opcodes/0x0074.md) | FriendDelReq | sub_482190 | SocialPackets::parseIdRequest | 4 | friends | PROVEN |
| [0x007A](opcodes/0x007A.md) | BlockAddByName | sub_481FB0 | SocialPackets::parseNameRequest | 2*(n+1) | blocks | PROVEN |
| [0x007B](opcodes/0x007B.md) | BlockDelReq | sub_482230 | SocialPackets::parseIdRequest | 4 | blocks | PROVEN |
| [0x007D](opcodes/0x007D.md) | SmallTalkReq | sub_482370 | SocialPackets::parseNameRequest | 2*(n+1) | smalltalk | PROVEN |
| [0x007E](opcodes/0x007E.md) | SmallTalkAccept | sub_4824B0 | SocialPackets::parseIdRequest | 4 | smalltalk | PROVEN wire, INFERRED accept |
| [0x007F](opcodes/0x007F.md) | SmallTalkDecline | sub_482410 | *SocialPackets::parseIdRequest | 4 | smalltalk | PROVEN wire, INFERRED decline |

### 3.2 C2S 0x0080 to 0x0135

| Op | Name | Client fn | Server symbol | Size | System | Status |
|---|---|---|---|---|---|---|
| [0x0080](opcodes/0x0080.md) | SmallTalkClose | sub_482550 | *SocialPackets::parseIdRequest | 4 | smalltalk | PROVEN wire, INFERRED close |
| [0x0081](opcodes/0x0081.md) | NoteSend | sub_4825F0 | SocialPackets::parseNoteSend | 2 strings | notes | PROVEN |
| [0x0084](opcodes/0x0084.md) | NoteMarkRead | sub_4826A0 | NONE, MIRROR_UI echo is correct here | 4 | notes | PROVEN |
| [0x0085](opcodes/0x0085.md) | NoteDelete | sub_482740 | NONE, MIRROR_UI echo is correct here | 4 | notes | PROVEN |
| [0x008C](opcodes/0x008C.md) | MissionGoalReached | inline sub_43AB00 | *MissionPackets::parseMissionComplete | 4 | mission | PROVEN |
| [0x008D](opcodes/0x008D.md) | PopupClose | sub_4827E0 | MissionPackets::parseLicensePanelClose | 4 | popup | PROVEN wire, DEAD sender |
| [0x008E](opcodes/0x008E.md) | ScreenDataRefresh | sub_483670 | *MissionPackets::parseMissionListRequest | 0 | mission | PROVEN sender, INFERRED reply set |
| [0x008F](opcodes/0x008F.md) | OpenMissionMenu | sub_483980 | MissionPackets::parseMissionMenuOpen | 0 | mission | PROVEN |
| [0x0090](opcodes/0x0090.md) | StartMission | sub_4835C0 | MissionPackets::parseMissionStart | 4 | mission | PROVEN |
| [0x0098](opcodes/0x0098.md) | GiftBuy | sub_482880 | ShopPackets::parseGift | 12+2 strings | gift | PROVEN |
| [0x009A](opcodes/0x009A.md) | GiftClaim | sub_4829C0 | NONE, routed to handleUseItem MISMATCH | 4 | gift | PROVEN |
| [0x009B](opcodes/0x009B.md) | GiftDelete | sub_482A60 | NONE, MIRROR_UI echo is correct here | 4 | gift | PROVEN |
| [0x009C](opcodes/0x009C.md) | GiftMarkRead | sub_482B00 | NONE, routed to handleUpgradeVehicle MISMATCH | 4 | gift | PROVEN |
| [0x00A1](opcodes/0x00A1.md) | ItemDrumPickup | sub_482BA0 | NONE | 4 | race gimmick | PROVEN |
| [0x00A3](opcodes/0x00A3.md) | LicenseTestSubmit | sub_482C40 | MissionPackets::parseLicenseTestSubmit | 12 | license | PROVEN |
| [0x00A6](opcodes/0x00A6.md) | BytesConsumedKeepalive | sub_477610 | GameServer::handleHeartbeat | 4 | session | PROVEN |
| [0x00A7](opcodes/0x00A7.md) | Reauth | sub_480500 | GameServer::handleSessionConfirm | 12+2*(n+1) | session | PROVEN |
| [0x00AA](opcodes/0x00AA.md) | GhostEnter | sub_482D10 | *GhostPackets::parseGhostEnter | 4 | ghost | PROVEN |
| [0x00AE](opcodes/0x00AE.md) | ReplayUploadCount | sub_425350 case 2010 | GhostPackets::parseUploadCount | 4 | ghost | PROVEN |
| [0x00AF](opcodes/0x00AF.md) | ReplayUploadChunk | sub_425350 case 2011 | GhostPackets::parseUploadChunk | 4+28*n | ghost | PROVEN |
| [0x00B0](opcodes/0x00B0.md) | GhostSubmit | sub_425350 case 2012 | GhostPackets::parseSubmit | 12 | ghost | PROVEN |
| [0x00B1](opcodes/0x00B1.md) | GhostStageBegin | sub_425210 | GhostPackets::parseStageBegin | 0 | ghost | PROVEN |
| [0x00B2](opcodes/0x00B2.md) | GhostFinalLap | sub_425350 case 2000 | GhostPackets::parseFinalLap | 0 | ghost | PROVEN |
| [0x00B4](opcodes/0x00B4.md) | ChatSend | sub_480990 | *SocialPackets::parseChatSend | 2*(n+1)+4 | chat | PROVEN |
| [0x00B5](opcodes/0x00B5.md) | WhisperSend | sub_480C00 | *SocialPackets::parseWhisperSend | 4+2*(n+1) | whisper | PROVEN |
| [0x00B7](opcodes/0x00B7.md) | Buy | sub_4841D0 | *ShopPackets::parseBuy | 12+2*(n+1) | shop | PROVEN |
| [0x00B8](opcodes/0x00B8.md) | SellDelete | sub_4844A0 | *InventoryPackets::parseDelete | 8 | shop | PROVEN |
| [0x00B9](opcodes/0x00B9.md) | EquipUse | sub_4842F0 | *InventoryPackets::parseInstall | 12 | garage | PROVEN |
| [0x00BA](opcodes/0x00BA.md) | Unequip | sub_4843D0 | *InventoryPackets::parseRemove | 8 | garage | PROVEN |
| [0x00CB](opcodes/0x00CB.md) | ConsumableUseNotify | sub_482DB0, sub_482F10 | ItemPackets::parseSwapTicket | 29 | race inv | PROVEN |
| [0x00CC](opcodes/0x00CC.md) | KartPartUseNotify | sub_482DB0, sub_482FE0 | *InventoryPackets::refreshOwnedPart path | 29 | garage | PROVEN |
| [0x00CD](opcodes/0x00CD.md) | HitAck or AbilityFire | sub_483180 | *ItemPackets::parseAbilityFire | 4 | race items | PROVEN |
| [0x00CF](opcodes/0x00CF.md) | ItemSlotReport | sub_483220 | *ItemPackets slot sync | 12 | race items | PROVEN |
| [0x00D0](opcodes/0x00D0.md) | WalletPoll | sub_4832F0 | *ShopPackets::parseCashPoll | var, mixed | wallet | PROVEN |
| [0x00D9](opcodes/0x00D9.md) | SelectCharacterAndKart | sub_4712A0 | NONE, MIRROR_UI echo | 68 | garage | PROVEN |
| [0x00ED](opcodes/0x00ED.md) | GachaRoll | sub_4830C0 | GachaHandler::handleRoll | 28 | gacha | PROVEN |
| [0x00F2](opcodes/0x00F2.md) | GimmickHitNotify | sub_4833A0 | *ItemPackets::parseAbilityClass | 4 | race gimmick | PROVEN |
| [0x00F5](opcodes/0x00F5.md) | ScenarioStageSelect | sub_483520 | ScenarioHandler::handleScenarioStageSelect | 4 | scenario | PROVEN |
| [0x00F8](opcodes/0x00F8.md) | ScenarioResultReport | inline sub_42E330 | ScenarioHandler::handleScenarioResultReport | 8 | scenario | PROVEN |
| [0x00FA](opcodes/0x00FA.md) | ScreenDataRequest | sub_483660 | *MissionPackets::parseFullStateRequest | 0 | screen init | PROVEN sender, INFERRED reply set |
| [0x00FE](opcodes/0x00FE.md) | QuestAccept | sub_483EB0 | QuestPackets::parseAcceptRequest | 4 | quest | PROVEN |
| [0x0100](opcodes/0x0100.md) | QuestDiscardReq | sub_483F50 | QuestPackets::parseDiscardRequest | 4 | quest | PROVEN |
| [0x0102](opcodes/0x0102.md) | QuestProgressReport | inline sub_401D90, sub_425350 | *QuestPackets::parseProgressReport | 4 | quest | PROVEN |
| [0x0105](opcodes/0x0105.md) | no sender | none | NONE | n/a | hole | NO SENDER |
| [0x0106](opcodes/0x0106.md) | no sender | none | NONE | n/a | hole | NO SENDER |
| [0x010A](opcodes/0x010A.md) | CarCraftOpenReq | sub_483680 | *CarCraftHandler::handleOpen | 0 | carcraft | PROVEN |
| [0x010B](opcodes/0x010B.md) | CarCraftSaveReq | sub_483690 | *CustomCarPackets::parseSaveRequest | 0x28+0x84*n | carcraft | PROVEN |
| [0x010E](opcodes/0x010E.md) | RoomCraftOpenReq | sub_483440 | *RoomCraftHandler::handleOpen | 0 | roomcraft | PROVEN |
| [0x010F](opcodes/0x010F.md) | RoomCraftSaveReq | sub_483450 | *RoomCraftHandler::handleSave | 4+0x30*n | roomcraft | PROVEN |
| [0x0110](opcodes/0x0110.md) | no sender | none | NONE | n/a | hole | NO SENDER |
| [0x0111](opcodes/0x0111.md) | no sender | none | NONE | n/a | hole | NO SENDER |
| [0x0112](opcodes/0x0112.md) | ShopExtendReq | sub_484570 | *ShopPackets::parseExtend | 12 | shop | PROVEN |
| [0x0113](opcodes/0x0113.md) | no sender | none | NONE | n/a | hole | NO SENDER |
| [0x0114](opcodes/0x0114.md) | CarCraftPresetRenameReq | sub_483790 | *CustomCarPackets::parseRenameRequest | 4+strlen+1 | carcraft | PROVEN |
| [0x0118](opcodes/0x0118.md) | RoomPasswordSet | sub_483840 | NONE | 8+2*(n+1) | lobby list | PROVEN |
| [0x011C](opcodes/0x011C.md) | Stage22OpenReq | sub_483970 | ScenarioHandler::handleMenuOpen | 0 | stage 22 | PROVEN |
| [0x011D](opcodes/0x011D.md) | Stage23OpenReq | sub_483990 | GameServer echo of 0x011D | 0 | stage 23 | PROVEN |
| [0x0121](opcodes/0x0121.md) | MissionCheckpointReached | sub_4839A0 | NONE, DATA_REQ16 swallow | 4 | mission | PROVEN |
| [0x0123](opcodes/0x0123.md) | PendantEquipReq | sub_483A40 | NONE | 4 | pendant | PROVEN |
| [0x0125](opcodes/0x0125.md) | BoostReadyFlag | sub_483AE0 | NONE | 1 | race boost | PROVEN |
| [0x012F](opcodes/0x012F.md) | RoomInviteBroadcastReq | sub_483B80 | NONE | 0 | room invite | PROVEN |
| [0x0130](opcodes/0x0130.md) | Option11Report | sub_483C10 | NONE, DATA_REQ16 swallow | 4 | options | PROVEN |
| [0x0132](opcodes/0x0132.md) | UserListPageReq | sub_483CC0 | SocialPackets::parseUserListPageReq | 4 | user list | PROVEN |
| [0x0133](opcodes/0x0133.md) | UserInfoReqById | sub_483D60 | SocialPackets::parseUserInfoReqById | 4 | social | PROVEN |

## 4. Payloads

One file per opcode under `opcodes/`, `opcodes/0x0040.md` for `0x0040`, both directions in the same file. `opcodes/README.md` is the index. `tools/opcode_pages.py index` rebuilds the index and the two tables above from the files, edit the opcode file, never the tables.

## 5. GAPS

This is the to do list. Everything here is either an unassigned opcode number, an opcode
with no server symbol, a field with no proven meaning, or a value the wiring had to invent.

### 5.1 Unassigned opcode numbers

No dispatcher case AND no client sender. Safe to reuse for nothing, they are simply not
part of this protocol. Sending one is silently dropped.

```
0x0000 0x0005 0x0006 0x0009 0x0015 0x0017 0x001A
0x004A 0x004C 0x004F 0x0050 0x0051 0x0052 0x0053 0x0055 0x0056
0x0059 0x005A 0x005B 0x005D 0x005E 0x0060 0x0061 0x006B 0x0075 0x007C
0x0089 0x00A8 0x00A9 0x00B3 0x00BD
0x00C7 0x00C8 0x00CA 0x00D1..0x00D8 0x00DA..0x00EC 0x00EF 0x00FD 0x00FF
0x0105 0x0106 0x0110 0x0111 0x0113 0x0127..0x012D 0x0134
```

Total 62 numbers. 0x0105 0x0106 0x0110 0x0111 and 0x0113 were settled on 2026-09-23 with the index byte of
each in the dispatcher table and the full sender census, the addresses are on their pages.
Protocol.h names sitting on some of them are fabricated:
`C_SCENARIO_MENU 0xC7`, `C_SCENARIO_CHAPTER 0xC8`, `C_SCENARIO_START 0xCA`,
`C_START_TUTORIAL 0xA9`, `C_PLAYER_PROFILE 0xB3`, `C_DRIFT_END 0xBD`,
`C_SELL_ITEM 0x6B`, `S_GACHA_ROLL_PAYOUT 0xB1`, `S_FRIEND_REMOVE 0xEF`.

### 5.2 Dead arms, real dispatcher case that does nothing

`nullsub_1 @0x0046F7A0` is shared by these S2C opcodes:

```
0x08 0x1F 0x20 0x24 0x36 0x37 0x38 0x43 0x48 0x66 0x6D 0x7E 0x7F 0x80
0x86 0x8B 0x8D 0x91 0x92 0x93 0x94 0xA0 0xBB
```

Of those, 0x6D, 0x7E, 0x7F, 0x80 and 0x8D have a REAL C2S sender, so never answer those
five with an echo, the client does nothing with it. The other 18 are dead both ways.

### 5.3 S2C messages with NO server symbol

These are real messages the client will act on and the server cannot currently produce.

| Op | Name | Why it matters |
|---|---|---|
| 0x002D | RoomListRowAdd | the lobby room list cannot be populated, PacketBuilder::chatMessage is a wrong label |
| 0x004D | ClientInfoProbe | no way to ask for the client info blob |
| 0x0084 | NoteMarkReadAck | MIRROR_UI echo happens to be correct, formalise it |
| 0x0085 | NoteDeleteAck | same |
| 0x0095 | GiftInboxReceivedList | received gift tab cannot be filled |
| 0x0099 | GiftAppendOne | no live gift arrival |
| 0x009A | GiftClaimAck | claiming a gift grants nothing |
| 0x009B | GiftDeleteAck | MIRROR_UI echo happens to be correct |
| 0x009C | GiftMarkReadAck | no builder |
| 0x00A1 | ItemDrumPicked | drum pickups are never relayed |
| 0x00C3 | TrackDefinition | track catalog cannot be sent, 0x0035 and 0x00AA both need it |
| 0x00C4 | ThemeDefinition | world folder resolution, needed by 0x00C3 |
| 0x00C5 | LicenseTestDefinition | license briefing and the 0x00A3 reward prediction |
| 0x00C9 | PlaySoundCue | no remote horn cue |
| 0x00CD | HitEffectApply | no hit token can be issued, C2S 0x00CD then echoes 0 |
| 0x00CE | SystemMessage | the localised system line channel is unusable |
| 0x00CF | ItemSlotSet | remote item slots never render |
| 0x00D9 | PlayerAppearanceUpdate | a garage change never shows on other clients |
| 0x00EE | KartPeriodUpdate | durability never propagates in race |
| 0x00F0 | CarLapFlagSet | lap latch cannot be set remotely |
| 0x00F1 | ServerClockSync | timed items are judged against the CLIENT clock |
| 0x00F3 | ScenarioDefinition | scenario menu is empty |
| 0x00F4 | ScenarioProgressList | scenario gating impossible |
| 0x00F5 | ScenarioStart | scenario cannot be entered, MIRROR_UI echo CRASHES it |
| 0x00F9 | ScenarioProgressAppend | no incremental scenario unlock |
| 0x0114 | CarCraftPresetRenameAck | rename never confirms |
| 0x0116 | WaitRoomSideListAdd | room side list never fills |
| 0x0117 | WaitRoomSideListRemove | same |
| 0x0118 | RoomLockState | room lock never propagates to the list |
| 0x011C | Stage22Ack | GameServer echoes it raw, formalise a builder |
| 0x011D | Stage23Ack | same |
| 0x011E | ServerRedirectStage11 | no race server handoff on the stage 11 path |
| 0x011F | RaceTimerArm | mission and license deadlines cannot be armed |
| 0x0120 | MissionCheckpointPath | mission checkpoints never appear |
| 0x0122 | MissionGo | mission briefing never closes |
| 0x0123 | PendantEquipAck | the pendant UI stays locked after one click |
| 0x0124 | CarCraftPresetRowUpdate | no single preset row push |
| 0x0125 | BoostReadyRelay | remote boost sprite never shows |
| 0x012E | ClientFileCheck | no client integrity check |
| 0x012F | RoomInvitePopup | the master invite broadcast has no delivery |
| 0x0131 | ItemRollStream | the item box RNG stream is never seeded |

### 5.4 C2S messages with NO server symbol

The client sends these today and nothing consumes them.

```
0x0012 OpenLobbyRequest and HostStartRace, MIRROR_UI only
0x0025 0x0029 NamePopupActionA and B, meaning INFERRED, MSG_WAIT hangs
0x0026 RoomMemberInfoRequest, answer with S2C 0x0028
0x0033 ReadyToggle, MIRROR_UI echo CRASHES the client
0x0035 TrackSelectRequest, MIRROR_UI echo CRASHES the client
0x003B LeaveRace, handled since 2026-09, answered with the lobby
0x004E DelayedAckFire
0x0062 LicenseTestStart, the chosen test is NOT on the wire
0x0065 TeamGaugeReport, MIRROR_UI echo CRASHES the client
0x006E JoinInvitedRoom, second join path
0x0084 0x0085 note mark read and delete, echo is correct by accident
0x009A 0x009B 0x009C gift claim, delete, mark read
0x00A1 ItemDrumPickup
0x00D9 SelectCharacterAndKart, MIRROR_UI echo CRASHES the client
0x00F8 ScenarioResultReport, MIRROR_UI echo CRASHES the client
0x0114 CarCraftPresetRenameReq
0x0118 RoomPasswordSet
0x0121 MissionCheckpointReached
0x0123 PendantEquipReq, no reply means a dead pendant UI
0x0125 BoostReadyFlag
0x012F RoomInviteBroadcastReq
0x0130 Option11Report
```

### 5.5 C2S routed to the WRONG handler

The route exists so the packet is consumed, then the wrong thing happens.

| Op | Real meaning | Bound to |
|---|---|---|
| 0x002F | JoinRoomRequest | GameServer::handleWhisper |
| 0x0039 | RoomMemberHostAction kick | GameServer::handleRaceFinish |
| 0x009A | GiftClaim | GameServer::handleUseItem |
| 0x009C | GiftMarkRead | GarageHandler::handleUpgradeVehicle |
| 0x00CB | ConsumableUseNotify | ScenarioHandler::handleScenarioComplete, can write DB rows |
| 0x00CC | KartPartUseNotify | ScenarioHandler::handleGetProgress |
| 0x00CD | HitAck | ScenarioHandler::handleGetChapterList, fires on every local hit |

### 5.6 Parsers that exist but are NOT ROUTED

```
0x0004 CharCreatePackets::parseCreateCharacter
0x0047 ItemPackets::parseUse           (item module now handles it, see wiring)
0x0049 ItemPackets::parseGrant         (item module now handles it)
0x004B ItemPackets::parseHomingLaunch
0x0057 ItemPackets::parseLockState
0x005C ItemPackets::parseTurtleLaunch
0x005F ItemPackets::parsePetReached
0x00CC PartStatPackets::parsePartUseNotify
0x00ED GachaPetPackets::parseRoll
0x010F RoomCraftPackets has no parser, the handler decodes inline
```

### 5.7 Fields with UNKNOWN meaning

Every field of this table was chased to its store and every reader of that store on the
client bytes, the opcode page carries the reader address for the settled ones. What is left
is the fields with no reader at all, listed with the store that was checked.

| Op | Field | State |
|---|---|---|
| 0x0046 | unknown_u32_b in the row | no reader in this build, row+0x4C of the 0x58 result row, only the sub_4B6850 write |
| 0x0013 | has_password | no reader in this build, 0xBCE224 has the handler write and the 0x0118 kind 0 write |
| 0x0013 | unused_2 unused_3 unused_4 | no reader in this build, 0xBCE214 0xBCE218 0xBCE21C have only the handler write |
| 0x0014 | +0x04 unused_04 | no reader in this build, 0xB23170 has only the handler write |
| 0x0073 0x0076 0x0077 | status_b, friend rec +0x28 | no reader in this build, the friend record readers read +0x04 and +0x24 only |
| 0x0087 | +0x00 +0x0C +0x24 +0x30 +0x34 | no reader in this build, all ten sub_450CA0 callers and both cached pointers checked |
| 0x008D | popup_context | the only sender belongs to popup kind 0x1F which nothing opens, the dword is its reset slot |
| 0x00A2 | third dword of each row | no reader in this build, every sub_4509B0 caller reads +0x00 and +0x04 only |
| 0x00C0 | +0x0C one byte, +0x18, stat wire 13 at +0xD8 | no reader in this build, container 0x01A22638, the shop copy 0xC60D94 and the car copy 0x33B0 checked, the 17 stat floats at +0xA4 are named by WIRE index in the page (wire k is car+0x3448 plus 4 k, settled 2026-09-15), every other dword of 0xC0 0xC1 0xC2 is named in its page |
| 0x00C3 | +0x3C | no reader in this build, +0x54 +0x58 are fog_near fog_far, +0x5C to +0x64 the lens flare position, see the page |
| 0x00C6 | f1 and f2 | no reader in this build, f3 to f6 are read by sub_454910 as period mode, period value, price and sale price |
| 0x00F3 | +0x00 +0x08 +0x18 +0x30 +0x34 | no reader in this build, +0x18 lands in 0xC70874 which has only the write |
| 0x00F8 | +0x04 flag | no reader in this build, 0xC70A34 has only the handler write, the success text is picked client side |
| 0x00FB | +0x10 +0x1C +0x20 +0x24 +0x28 | no reader in this build, the quest def readers are sub_402210 and sub_43CCC0 |
| 0x00FB | +0x14 +0x18 | drawn as bare numbers on the quest detail panel, the label sits in the art |
| 0x010C | +0x14 | not chased in this pass |
| 0x0116 | kind | only the value 6 draws, no other reader |
| 0x011A vs 0x011B | which is bulk and which is a live grant | one handler for both, the split is a server convention |
| 0x0135 | +0x00 +0x14 +0x18 | read into stack locals the handler never uses |
| 0x0135 | ctx_a ctx_b | stored at 0x11B44E8 and 0x11B450C, no read xref |
| 0x001C | kart row +0x14 +0x18 +0x1C | no reader in this build, the loadout sub_490A70 reads slots 0 1 2 6 7 only |
| 0x001E | part row +0x08 | no reader in this build, the client only echoes it in C2S 0x00CC |

Settled in this pass, see the pages: 0x0045 ping_ms, 0x002D player_count max_players game_mode
has_password channel playing_flag time_left_ms, 0x0013 has_password, 0x0087 reward_mileage
reward_exp, 0x00CB the owned item row verbatim, 0x00F3 track_id entry_fee reward_mileage
reward_exp reward_key and the three string keys, 0x00FB enabled quest_index theme_id
goal_count, 0x0103 required_pendant_key, 0x010C badge, 0x0119 every dword, 0x011A
pendant_instance_id, the owned rows price_key in 0x001B 0x001C 0x001D 0x001E 0x0104 and
the kart row part and item slots.

### 5.8 Values the server had to invent

Marked `unproven` in the tree, in a comment, a function name or a migration note. Each one is a live risk.

| Area | Invention | Consequence if wrong |
|---|---|---|
| progression | the whole 55 row level_curve cum_exp ladder | wrong level for a given exp. Only the LENGTH is backed, Lv_icon_001..055 ship. No shipped table and no capture carries the numbers |
| progression | client draws the level 50 icon for 51..55 | client side constants at 0x00442622, 0x00443457, 0x0044345C, server cannot fix it |
| inventory | SWAP_ITEM_STARTER_USES = 5 | policy only, the client just tests remaining > 0 |
| inventory | legacy 7 int stat row to the 4 garage bars | wrong bar values, no client read pairs them |
| inventory | def_item_wire.use_type for item key 1000, seeded 0 | 0 is the branch that reads no tail so it cannot desync |
| inventory | def_kart_part_wire model to slot pairing | wrong mesh in the wrong socket, cannot desync |
| standings | the 0x0045 third dword 30 | SETTLED, it is a ping in ms, sub_447AE0 buckets it to Icon/link_0..4 and 30 draws the best bar, kept |
| spawn | theme id 90 for the Race folder | cosmetic, the two Race tracks get the default BGM |
| spawn | reserved theme 20000000 bound to the Mission folder | INFERRED from track.COL having no CHECK faces |
| spawn | reserved theme 30000000 bound to a Battle folder that does not exist on disk | inert until a battle world exists |
| spawn | mission track ids 20000001..20000005 | server side only. The client never receives a mission track id, stage 0x19 keys the loaded world on the mission id 0xD09558 and sub_488000 builds World/Mission/%s from the 0x87 def +0x38 world_name |
| spawn | lap_count 3 for every track row | picker preview only, the raced value is overridden |
| spawn | track +0x44 +0x48 +0x4C all 0 | 0 is the only value proven SAFE for the room list filter |
| spawn | camera_a b c seeded 0.4 0.6 90.0 | those are the client own .data initialisers, per track values are lost |
| spawn | respawn audit tolerance 40 world units | audit only warns, it never rejects |
| ghost | which recorded track a quest ghost plays back | SETTLED on the client, S2C 0x00F3 +0x0C is the 0xC3 track id and stage 0x11 loads it from 0xC70A4C. scenario_def still has no column for it so the builder sends 0, open until the table grows one |
| social | status_b activity code 0 lobby 1 room 2 racing | zero impact in this build, no reader of rec+0x28 anywhere, kept |
| social | MSG_ANTI_CHAT and MSG_FRIEND_NOUSER as the 0x0126 keys | a wrong key prints as literal text, never crashes |
| social | the mirrored S2C 0x007D back to the requester | mechanism is forced, the original behaviour is not proven |
| social | free text on the invite answer notice and the smalltalk notices | display only |
| mission | the S2C reply set for C2S 0x00FA and C2S 0x008E | wired to 0x000A and 0x0088, both proven safe to resend |
| mission | mission currency mapping onto gold, exp and cash | HALF SETTLED, the labels are code: def +0x1C composes UNIT_MILEAGE and +0x20 composes UNIT_EXP in sub_43C060, the scenario HUD sub_4B5A00 draws the same pair. S2C 0x8C carries gold_after and exp_after. The server now pays +0x20 as exp and +0x1C as gold, gold is the inferred half since no client code adds the number to a wallet |
| mission | pet condition key space, seeded key 1 | SETTLED, the key is a 0x0119 pendant key, sub_460A10 refuses the buy unless sub_451290 finds it in the 0x011A owned pendant list. Column is shop_definition.required_pendant_key, the pet_condition table is only a fallback |
| mission | time_limit_ms 120000 to 180000 | must stay non zero, the deadline is now plus this |
| craftquest | carcraft stat_block_hex empty, all 17 floats zero | factory parts are cosmetic, no shipped file carries them |
| craftquest | quest theme step gate, publish theme T only after T minus 1 is complete | the client NEVER locks a theme button, this is pure server policy |
| craftquest | chassis index 1..4 to folder names | cosmetic, and it disagrees with RE_PARTSTATS_CONTENT.json |
| craftquest | FLOOR and EFFECT loc key ordering | cosmetic |
| craftquest | room_object_def.max_placeable per key | only the per category cap of 50 is proven |
| craftquest | room_object_def badge 1 hot 2 new | SETTLED, sub_41A160 draws UI_Shop_itembox_hot on 1 and UI_Shop_itembox_new on 2 |
| craftquest | quest_def detail_value_lower and detail_value_upper as rewards | the two dwords are drawn as bare numbers on the detail panel, what the label says is in the art, the values are ours |
| craftquest | room decor weather 0 in the 0x0013 refresh | 0 is the existing default, not a proven value |
| pendant | pendant_instance_id 0 on every 0x011A row | WRONG for the reward path, sub_451250 erases the FIRST row whose +0x00 matches, the login burst must send a unique id per owned row |
| shop | whether the C2S 0x00D0 launcher token equals our minted session token | refusing on a mismatch would kill every live session, so it warns and continues |
| shop | price_base 0 for categories 5 and 6 | content gap, everything is free |
| shop | no shop_definition rows for category 3 | category 3 buys answer UnknownDefinition |
| item | effect lifetimes other than code 100 | inherent, they end on client side managers, so late viewers are never handed them |
| item | server side hit arbitration for bots | proven IMPOSSIBLE, the spawn xyz is never stored in the tested fields |
| item | managerCap returns 0 for kinds 0 1 4 5 10 12 16 17 19 20 21 | those manager loops were never read, 0 means skip the check |

### 5.9 Known live server bugs to fix

| Where | Bug |
|---|---|
| MIRROR_UI set | zero payload echoes of 0x25 0x33 0x35 0x47 0x49 0x4B 0x57 0x5C 0x5F 0x65 0x69 0x6A 0xB5 0xCF 0xD9 0xED 0xF5 0xF8 0xFE make the client read past the frame end |
| PacketBuilder::shopUpdate | builds 0x6A as {i32 gold, i16 cash}, the exact MOTION_BLOCK shape, and it is LIVE after a sell |
| PacketBuilder::notification | builds 0x7D as {i32 type, wstring msg}, the exact SmallTalkInvite shape, and it is LIVE |
| PacketBuilder::giftSendConfirm | writes senderId and recipientId into the two 0x0098 money slots |
| PacketBuilder::entityStrings | writes the three 0x0119 strings as UTF16, the client reads ASCII |
| PacketBuilder::gachaTransactionResult | targets 0x0134 which has no handler, and has the 0x0135 field order wrong |
| GameServer 0x0063 | createRoomResponse sends a room id where the client reads a game mode index 0..4 |
| Protocol.h 0x0096 0x0097 | S_GIFT_INBOX_RECV and S_GIFT_INBOX_SENT are swapped |
## 6. CONFLICTS

Where two sources disagreed. Each entry gives both readings and which one the evidence
favours. The favoured reading is what section 4 uses.

### C1. 0x0007 and 0x00A7 profile blob sub offsets, shifted by 4

Reading A, the 0x00 to 0x3F sweep:
nickname +0x48A, band +0x4A4, level +0x4A5, padding +0x4A6, exp +0x4A8, astro +0x4AC,
gold +0x4B0, selchar +0x4B4, selkart +0x4B8, exp_floor +0x4C0, exp_next +0x4C4,
title +0x4C8.

Reading B, the 0x80 to 0xBF sweep:
nickname +0x486, grade +0x4A0, level +0x4A1, exp +0x4A4, astro +0x4A8, gold +0x4AC,
selchar +0x4B0, selkart +0x4B4, exp_next +0x4C0.

FAVOURS B. Proof is the absolute VAs other packets write. Blob base is 0x01A20668.

```
0x01A20B08 - 0x01A20668 = 0x4A0   license grade, written by S2C 0x00A4
0x01A20B09 - 0x01A20668 = 0x4A1   level
0x01A20B0C - 0x01A20668 = 0x4A4   exp, written by S2C 0x003C and 0x00F8
0x01A20B10 - 0x01A20668 = 0x4A8   astro, written by S2C 0x00B7 field 3
0x01A20B14 - 0x01A20668 = 0x4AC   gold, written by S2C 0x008C, 0x0090, 0x00B7, 0x00F5
0x01A20B18 - 0x01A20668 = 0x4B0   selected character instance
0x01A20B1C - 0x01A20668 = 0x4B4   selected kart instance
```

Reading A also puts title at +0x4C8 which is one dword PAST the end of a 0x4C8 blob.
Note the same byte at +0x4A0 is called channel_level_band by A and license_grade by B.
Both are right, it is one byte used as a licence grade and as a channel band gate.

OUR LOGIN SERVER IS A THIRD READING and it is wrong. `HandshakeHandler::sendSessionInfo` in
`server/login/src/handlers/HandshakeHandler.cpp` writes gold at blob +0x4A4 where B has exp,
0 at +0x4A8 astro, and cash at blob +0x4AC where B has gold, then the selection pair at
+0x4B0 and +0x4B4 which does match B. Our game server blob,
`CharCreatePackets::profileBlob`, follows B. Fix the login server, not the pages.

### C2. Protocol.h PlayerInfo struct

Protocol.h inverts the two selection ids, calls +0x4A4 gold when it is exp, calls +0x4A5
isGM when it is level, and calls the last three dwords wins, losses and rankPoints when
they are exp floor, exp next and title key.
FAVOURS the disassembly. Protocol.h PlayerInfo is wrong on six fields.

### C3. What id S2C 0x0028 and 0x0072 blob+0x00 carry

Reading A: the ACCOUNT id, i.e. profile blob +0x000, because the handler compares it
against `dword_1A20668`.
Reading B: player_id.
FAVOURS A on the mechanism, the compare target is 0x01A20668 which is the FIRST dword of
the 1224 byte profile blob, NOT the player_id at 0x01A20658 that 0x0007 payload +0x000
delivers. Whatever it is named, the server must echo the value it put at blob +0x000 or the
Add Friend button appears on the player own profile.

### C4. 0x0096 and 0x0097 gift tabs

Protocol.h: `S_GIFT_INBOX_RECV = 0x96`, `S_GIFT_INBOX_SENT = 0x97`.
Ghidra on FUN_0047bd80 and FUN_0047be00: 0x96 fills 0x01A61560, 0x95 and 0x97 both fill
0x01A5FC80.
FAVOURS Ghidra. The two names are SWAPPED. 0x96 is the SENT archive, 0x95 and 0x97 are the
RECEIVED tab and are true aliases.

### C5. Which opcode is the server redirect

Protocol.h: `S_SERVER_REDIRECT = 0x54`.
Dispatcher: three DIFFERENT redirects exist with three different shapes.

```
0x0019  sub_479340   ASCII cstr host, u32 port, u32 mode
0x0054  sub_47AA00   u32 unknown, ASCII cstr ip, u32 port
0x011E  sub_47E9B0   ASCII cstr host, u32 port, target hardcoded 11
```

FAVOURS the dispatcher. `S_SERVER_REDIRECT = 0x54` is only correct for the game server
handoff, `CharCreatePackets::serverRedirect` is 0x0019, and 0x011E has no name at all.

### C6. C2S 0x00F5, scenario or quest

Registry reading: ScenarioStageSelect, `sub_483520` called by `sub_437F30`, the ScenarioMenu
stage click, u32 is a scenario_key.
Ghost module reading: quest ghost start, same sender, same caller, u32 is a questIndex.
FAVOURS the registry. The S2C 0x00F5 handler resolves the key with `sub_452E30` against
0x01A64720, the SCENARIO definition container from 0x00F3. The quest containers are
0x01A66738 for definitions and 0x01A67D20 for state and neither is touched.
Action: `ghost_quest_replay` should key on the scenario key, not on a quest index.

### C7. 0x0045

Older notes called it ITEM_USAGE.
Disassembly: it writes a stride 3 standings table and the index of my own id in that table
IS my 0 based rank, which the loot roll consumes.
FAVOURS standings. There is also NO C2S 0x45 anywhere, so `CMD::C_ITEM_USE = 0x45` and the
route to handleItemUse are a phantom that can never fire.

### C8. 0x0031

`PacketBuilder::position` treats it as 3D motion.
Disassembly: `sub_407600` walks the 136 byte stride 512 entry room LIST table and writes the
two dwords 0x2D delivered. It never touches the car array nor the interpolation queue.
FAVOURS the room list reading. Settled twice independently.

### C9. Lookup key offset of the 0xC0, 0xC1 and 0xC2 catalogs

Earlier sweeps: rec+0x00.
Range 0xC0 to 0xFF sweep: rec+0x08, because the finder starts at `this+3` with the record
base at `this+1`.
FAVOURS rec+0x08 for all three.

### C10. 0x00C5 field count

ARBITRATION: 12 i32 then ONE trailing cstr.
Disassembly: THIRTEEN u32 then TWO trailing cstr, record is 0xA4.
FAVOURS thirteen and two. A short read here desyncs every packet behind it.

### C11. 0x006C field 6

RE_SWEEP2: field 6 is an inviter name.
Disassembly: field 2 lands at popup this+24196 which is concatenated with MSG_INVITE, and
field 6 lands at popup this+24264 which is staged through a `wchar_t[10]` password buffer.
FAVOURS field 2 inviter name, field 6 room password. `SocialPackets::roomInvite` ordering
is already right.

### C12. Which opcode `sub_47CC20` answers

Protocol.h: `S_GACHA_ROLL_PAYOUT = 0xB1`, comment names sub_47CC20.
Dispatcher: sub_47CC20 is case 176 which is 0xB0. There is no case 177.
FAVOURS 0xB0. `PacketBuilder::gachaRollPayout` currently sends to a dead number.

### C13. The 0xEF, 0xF0, 0xF1 off by one chain

Protocol.h: `S_FRIEND_REMOVE = 0xEF` names sub_47D930, `S_FRIEND_RECORD = 0xF0` names
sub_47D970.
Dispatcher: sub_47D930 is case 240 which is 0xF0, sub_47D970 is case 241 which is 0xF1,
and 0xEF has no case at all.
FAVOURS the dispatcher. Both Protocol.h names are one number low AND both are the wrong
subsystem, 0xF0 is a car lap flag and 0xF1 is a struct tm clock sync.

### C14. 0x00C9 versus 0x00CA

Protocol.h: `S_FRIEND_STATE_UPDATE = 0xCA` names sub_47CD20.
Dispatcher: sub_47CD20 is case 201 which is 0xC9, and there is no case 202.
FAVOURS 0xC9, and it is a per player sound cue, not a friend message.

### C15. Roomcraft save opcode

An older doc: 0x0111.
Disassembly: `PUSH 0x10F` at 0x00483484.
FAVOURS 0x010F. 0x0110, 0x0111 and 0x0113 are all unassigned.

### C16. C2S 0x00CC flag 0 sender

`PartStatPackets.h` header comment: sub_482E60.
Disassembly: there is no sub_482E60 in this build, the flag 0 sender is sub_482DB0 and the
flag 1 sender is sub_482FE0.
FAVOURS sub_482DB0 and sub_482FE0.

### C17. Carcraft chassis index to folder

`RE_PARTSTATS_CONTENT.json`: indexes 1 and 2 one way.
craftquest wiring: 1 Striper, 2 Circler, the opposite pairing.
UNRESOLVED. Only index 0 Firedragon is proven, from COVER_1000_TITLE and
CAR_CHASSIS_01_TITLE. A wrong pairing shows the wrong display name and cannot desync.

### C18. C2S 0x00CD naming

Range 0xC0 to 0xFF sweep: HitAck, echoes the token S2C 0x00CD delivered.
Item module: AbilityFire, clears the stale protection view and rate checks the burst.
NOT A CONFLICT ON THE WIRE, both agree the payload is one u32 and both agree the call site
is `sub_4B82A0` for gimmick or ability classes 0 4 5 6 7 23. The registry keeps HitAck
because the value sent is provably the global that S2C 0x00CD wrote.

### C19. Status vocabulary between the five sweeps

Sweeps 1 to 3 marked a missing dispatcher case as PROVEN with the name NO CASE. Sweep 4
marked the same situation NULLSUB. Sweep 5 marked it UNKNOWN.
NOT A DATA CONFLICT. Normalised in this registry: NO CASE for a missing dispatcher arm,
NULLSUB for a real arm that is `nullsub_1`, UNKNOWN for a number with neither a handler nor
a sender. Section 5.1 lists the unassigned numbers regardless of which word a sweep used.
