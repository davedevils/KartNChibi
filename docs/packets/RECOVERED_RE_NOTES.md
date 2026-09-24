# Reverse engineering notes recovered from source comments

Kept here when the source comments moved to one short line each.
Every block below cited a client address that appears nowhere else.

## server/game/include/handlers/CarCraftHandler.h

Line 20

```
@brief CarFactory stage 18 handler
Screen order is fixed by sub_430EF0 which snapshots the preset and part
containers at stage init: every 0x0107 and 0x0109 goes out BEFORE the
0x010A ack. 0x0108 part definitions are a blind append (sub_44F9F0) so they
ride the login push exactly once per session and never again.
The handler also owns the custom_car[0x3C] block that rides in the S2C 0x0021
room member tail and the S2C 0x003E grid racer tail. sub_490A70 copies that
block at +686336 and only feeds it to sub_48F710 when the kart catalog row
dword 5 is 1, which is the factory car scheme.
PART DEF CONTAINER BASE IS 0x01A7932C, NOT 0x01A79320. Every call site loads
MOV ECX,0x1a7932c. sub_44FBC0 keys on base+0xC which is rec+8, stride 0x120,
count at base+0x12004, capacity 256, and it returns base+4+i*0x120. The
region reads all zero in the image so every number comes off the wire.
RECORD LAYOUT, fixed by the sub_480210 read order and confirmed by the users:
rec+0x08 part key sub_44FBC0 key and the icon id sub_4436B0 draws
rec+0x14 model dir char[33]
rec+0x35 name key sub_42FE20 hands it to sub_4E1B70
rec+0x56 info key char[34]
rec+0x78 0x44 stats 17 floats
sub_48F710 adds the first ten floats rec+0x78 to rec+0x9C as
stat * (1 + level / 50) with INTEGER division, so every level below 50
contributes the bare stat, and adds the last seven rec+0xA0 to rec+0xB8 flat.
The destination is the racer block at +0xA7940 which sub_490A70 memsets to
zero over 0x44 bytes first, and the physics in sub_49AA90 and sub_49C0D0
then read it.
NOTHING SHIPPED CARRIES THOSE 17 NUMBERS. Counted with
find Data Define -type f ( -iname "*.ini" -o -iname "*.txt" )
DevClient holds 478 text files, 447 .ini plus 31 .txt. 443 of them sit under
Data/Public/World as per track geometry .ini and the other 35 are per body
_desktop.ini files, gimmick readme.txt notes, Data/Eng/def_taboo.txt and
Data/Public/Image/Icon/paint color.txt. None is a numeric part table. The
same holds inside pak001.dat, whose 21044 path strings were enumerated and
whose only non World text entries are those same desktop.ini and readme.txt
files. The two .xlsx files that ship are Swamp gimmick positions.
What DOES ship is Define/Eng/def_trans_index.txt with 405 PART_<key>_TITLE
and PART_<key>_INFO pairs whose def_trans_message.txt text states each
intended bonus in prose with percentages, for example "Blocks 20% of Bazooka
attacks". That prose is the authoring reference for the stat block and the
display key must be the shipped PART_<key>_TITLE form.
```

## server/game/include/handlers/CharCreateHandler.h

Line 1

```
@file CharCreateHandler.h
@brief login handshake, the RegistDriver popup and every post creation stage jump
Flow for a fresh account:
C2S 0x0007 -> S2C 0x0007 login accept -> S2C 0x00BE -> N x S2C 0x00BF -> S2C 0x0011
-> S2C 0x0003 RegistDriver popup
C2S 0x0004 {u32 driver_key, wstring nick} -> S2C 0x0004 success -> S2C 0x000E channel list
Flow for an account that already owns a character:
C2S 0x0007 (or C2S 0x00A7 after a redirect) -> S2C 0x0007 / 0x00A7 -> S2C 0x000E
HARD PRECONDITION, S2C 0x0003 divides by the 0x00BF driver catalog entry count in
sub_473730, so an empty catalog is a divide by zero crash. openCreateScreen always
pushes S2C 0x00BE plus the whole 0x00BF burst first and refuses to send 0x0003 when
the burst came out empty.
SCREEN ACK RULE, the client stage init snapshots the network containers BEFORE it
closes the MSG_WAIT modal, so everything a stage needs must ride ahead of the stage
opcode. S2C 0x000E is itself the stage 4 ack, sub_4793F0 parses every channel row
then calls sub_404410 stage 4 then sub_4538B0, so nothing may follow it for that
screen. S2C 0x0004 is NOT a stage ack, sub_479230 calls sub_4538B0 first then only
raises MSG_SAVE_DONE. Stage 5 is raised by S2C 0x0011 in sub_4793C0 alone.
RESULT CODE RULE, S2C 0x0004 accepts only 0 1 2 3. Any other value falls out of the
sub_479230 switch into MSG_UNKNOWN_ERROR type 2 which closes the socket. The full set
is 1 MSG_INVALID_NICK, 2 MSG_ALREADY_REGIST, 3 MSG_INVALID_NICK again, so there is no
driver specific refusal at all and a bad driver key can only reuse 1 or 3. All three
go through sub_4707B0 with event arg 2, and sub_470260 posts that as WM_USER 0x7E8
wparam 2 on dismiss, which the stage 2 3 and 5 window procs turn into sub_473730.
That is why the popup reopens the creation screen by itself and 0x0003 must NOT be
resent.
Wire facts from docs/packets/RE_SWEEP2_2026-08-17.json domain charcreate, with the
verdict CORRECTIONS applied (0x000E is 16 fixed bytes per entry not 12, S2C 0x0062
closes the modal before the stage init, target_stage is not a closed set).
```

## server/game/include/handlers/ItemHandler.h

Line 1

```
@file ItemHandler.h
@brief in race item system, slots effects protections and the item wire
Owns the SERVER side of every proven in race item opcode. RaceHandler keeps the
motion and lap wire, this file keeps the item wire and the item state. Nothing
here starts a thread or a timer, tick() is driven by the orchestrator frame.
C2S routed here
0x47 item use -> S2C 0x47 spawn, self echo rules below
0x49 grant report -> S2C 0x49 grant, everyone but the sender
0x4B homing launch -> S2C 0x4B, EVERYONE, the shooter has no local launch
0x57 lock state -> S2C 0x57 to the TARGET only, pure relay
0x5C turtle launch -> S2C 0x5C, everyone but the sender
0x5F pet reached -> S2C 0x5F, pure relay to everyone
0x69 hit report -> S2C 0x69, everyone but the sender
0x6A start boost -> S2C 0x6A, everyone but the sender
0xCB swap ticket -> owned_item base_key 1000 decrement, no S2C
0xCD ability fired -> state only, no S2C case exists in the client
0xCF slot array -> S2C 0xCF, everyone but the sender
0xF2 ability class -> state only, EXCEPT class 10/11 (a real shield
absorbed the hit) which now also broadcasts S2C
0xCD shield absorb token, everyone but the
sender, see ItemPackets::shieldAbsorbToken
S2C 0x6A IS THE START BOOST, proven this pass, it is NOT a shop opcode and it
is NOT junk. sub_4B1E50 arms a window when the countdown reaches two, a press
between 900 and 1050 ms later sends C2S 0x6A with 1150 minus that elapsed, so
the wire value is the MILLISECONDS LEFT until the boost fires. sub_49E8A0
writes value at car+0xA78C4, flag 1 at car+0xA78C0 and the clock at car+0xA78C8.
sub_49ECD0 fires the remote boost after value ms then clears the flag 700 ms
later. The S2C 0x40 handler sub_47FD30 SKIPS every relayed sample while that
flag is set, so a bogus value freezes the car for exactly that long. Clamp it.
SELF ECHO. Online the client does not spawn locally for 2 3 4 5 7 8 9 11 13 14
15 17 18 19 20, so S2C 0x47 must include the sender for those kinds. It DOES
spawn locally for 0 1 10 12 16 21. Kinds 0 1 6 10 16 have no S2C 0x47 case at
all and are never relayed.
MID STREAM BAIL. S2C 0x47 0x69 and 0x6A resolve the player id before reading
the rest, so an id that is not live in the client roster leaves bytes in the
stream and desyncs the whole connection. Every send here is gated on the id
being a live room participant.
HIT REPORTING IS PARTIAL. Only six client sites emit C2S 0x69, covering
gimmicks 100 and 300, storm 100, hive 700, ice 1000 and flash 1100. Spike bomb
dung turtle rocket and magnet report NOTHING because their collision runs on
every client for every car. The server cannot arbitrate item geometry.
```

## server/game/include/handlers/PartStatHandler.h

Line 73

```
@brief Durability a repair scroll gives back. DELIBERATE SERVER POLICY.
This constant and RACE_DURABILITY_COST are policy, not missing reverse engineering.
The client never produces either number. Kart durability lives at owned-kart record
+0x2C mode and +0x30 value, dword index 11 and 12 of a 0x38 byte record.
Three functions READ the value and none of them subtracts:
sub_40D650 @0x40D650 (KnC-new.exe.c:143101, a11[12])
sub_4795A0 @0x4795A0 (:220247, *(v2 + 48))
sub_42AD20 @0x42AD20 (:164852 :164856 :164857), the garage bar. Its own mode 3
guard is :164842 and six screens call it, guarded at :149729 :167999
:168008 :168018 :200754 and :216191
The first two only raise MSG_DURABILITY_LOW at <= 10 or MSG_DURABILITY_ZERO at <= 0.
The repair click path sub_412BC0 @0x412BC0 (:146882) gates on item use type 4..7 and
period mode 3 and then opens a confirm dialog, computing no amount.
The value is WRITTEN two ways, so an earlier "exactly one instruction" claim here was
false. FUN_0044f2d0 @0x44F2D0 copies 14 dwords per insert, so every one of the twelve
sub_44F2D0(dword_1A576D8, ...) inserts writes +0x30 whole. The only ARITHMETIC write
anywhere is the 500 clamp at :164856. Searched byte offset 48, dword index 12, both
container name forms dword_1A576D8 and &unk_845228 + this, and every subtraction
shape on either literal: no decrement exists. Tune these two here.
```

Line 131

```
@brief 0x00C0 record +0x10 kart class code, ELEVEN read sites, only seven compare.
sub_490A70 @0x490A70 copies the whole 0x140 definition record into the car struct at
+13220 (KnC-new.exe.c:236660), so record +0x10 lands at car +13236 == dword 3309.
Two earlier claims here were wrong. The first said three read sites, all in
sub_49C0D0. The second said seven, all comparisons. Both searched only offsets INTO
THE CAR COPY. The field is also read straight off the definition record pointer,
where IDA writes it as a4[4] or *(a1 + 16) and none of 13236, 3309 or 6618 appears.
Car copy, seven comparisons:
sub_49C0D0 @0x49C0D0 local car, byte form :244487 == 2 :244539 == 2 :245226 == 5
sub_48E6A0 @0x48E6A0 remote car, index form :235039 == 2 :235078 == 2 :235537 == 5
sub_49A8A0 @0x49A8A0 lean helper, index form :242534 != 2
Definition record, four table lookups, NOT comparisons. All four call
sub_49A2A0 @0x49A2A0, which is a WHEEL COUNT map, not a lean switch:
class 2 gives 4, class 3 gives 6, class 4 gives 0, every other value gives 4
sub_490A70 :236663 stores it at car +2528, sub_498DE0 @0x498DE0 :241348 uses it as
the O_WHEEL%02d bind loop bound and falls to the O_SM%02d sub model path when it is
zero, sub_4A5ED0 @0x4A5ED0 :250785 and :251391 repeat that in the garage preview.
IDA xrefs_to 0x49A2A0 returns exactly those four callers.
So codes 3 and 4 are LIVE and this constant set does not name them. Class 3 is a six
wheel chassis and class 4 is a wheelless one. The == 2 pairs swap which body lean term
stat 6 and stat 7 scale, the == 5 test runs an extra per frame spin driver, and
sub_49A8A0 RETURNS 0.0 for every class except 2, so class 2 is the only class that
ever produces a nonzero steer lean term at all. PLAIN is still the correct default for
a prebuilt kart because 0 takes both default arms, the plain lean branch and the four
wheel bind.
```

Line 194

```
@brief Period modes. Mode 2 is a USE COUNT, not a clock.
Owned part records live in the container at 0x1A584E0. FUN_00450d20 @0x450D20 copies
7 dwords per insert so a record is exactly 0x1C bytes, the same 0x1C the client echoes
back on 0x00CC. Mode is rec+0x10 (index 4), value is rec+0x14 (index 5).
sub_4B8580 @0x4B8580 does `--*(rec+20)` at KnC-new.exe.c:263035 when the local car
takes a hit, then sends 0x00CC. That is the only ARITHMETIC write to the value but it
is NOT the only write: every sub_450D20 insert rewrites the whole 0x1C record.
An earlier claim here said the mode word has three reads. Wrong twice over. The two
`this` relative lookups ARE this same container, not a different one: sub_482DB0 and
sub_482FE0 do LEA ECX,[ESI+0x846030] and ESI is 0x12124B0 at all three call sites
(0x4090e8, 0x41142b, 0x4b8635), and 0x12124B0 + 0x846030 == 0x1A584E0. And there are
eight reads, not three:
`== 2` :226708 sub_482DB0, :226781 sub_482FE0, :263034 sub_4B8580
truthiness :147448 sub_413950, :147733 sub_413D30, :148066 and :148126 sub_414630,
:148402 sub_414BF0, all the same icon badge draw
No read compares the mode to a clock and none tests 3, so the server still owns no
countdown, it only persists what the client reports. Mode 3 is kart durability and is
read only, see the policy block above.
```

Line 220

```
@brief Tail the client holds after an S2C 0x6A delayed launch fires, PROVEN.
sub_47AFE0 @0x47AFE0 only parses, it reads a u32 player id and an i16 and hands both
to sub_49E8A0 @0x49E8A0, which is the function that actually stores the delay at
car+0xA78C4, sets the state word at car+0xA78C0 to 1 and stamps car+0xA78C8
(KnC-new.exe.c:245325-:245328). sub_49ECD0 @0x49ECD0 then runs the state machine per
frame: state 1 waits the delay, fires the launch sub_49EAE0 @0x49EAE0, restamps and
moves to state 2; state 2 clears to 0 after 0x2BC == 700 ms at :245507. Searched byte
offsets 686272 686276 686280 and dword indices 171568 171569 171570: nothing on the
wire clears it, the only other write is the whole car reset in sub_48DB30 @0x48DB30
at :234239. While the state is non zero both the 0x40 relay and the 0xA5 position fix
skip that car entirely.
```

Line 454

```
@brief S2C 0xA5 position only correction to a list of viewers.
sub_47C830 @0x47C830 reads a count byte then per entry a u32 player id and the
8 byte packed vec3, nothing else. Re read line by line: the only writes it makes are
byte_1B19A6A[stride * car] = 0 and the interpolator at unk_1B1C3D8 + stride, so it
never touches car yaw at +12832 or the state word and the car keeps its heading and
whatever animation it was in. The position is not written either, it is handed to the
same interpolator the 0x40 relay uses with a fixed 1000 ms glide. The skip test is
dword_1BC0950[171160 * car] and 0x1B19090 + 0xA78C0 == 0x1BC0950, so that IS the
S2C 0x6A motion block state word.
```

## server/game/include/handlers/QuestHandler.h

Line 1

```
@file QuestHandler.h
@brief quest catalog and per character quest state, client stage 26
Push at character enter: S2C 0xFB x N then S2C 0xFC once.
Deltas afterwards: S2C 0xFE accept, 0x100 discard, 0x101 complete, 0x102 progress,
plus S2C 0xFB x 4 when the next theme step unlocks.
C2S: 0xFE accept, 0x100 discard, 0x102 progress report.
Stage 26 sends no screen open opcode, the top bar switches stage locally, so there is
no ack to race. Both client containers must already hold the data before the player
can reach the button, which is why the push happens at enter.
The 0xFB catalog container is append only (sub_452180) so a theme that unlocks mid
session is sent one definition at a time and a row is never sent twice.
HARD CRASH CONSTRAINT enforced on every publish: quest_index MUST equal
4 * theme_id + row with row in 0..3 and every theme must be contiguous from row 0.
A gap or an out of range row makes sub_452240 return 0 and TWO sites then deref
+44 with no null test: the stage 26 list at KnC-new.exe.c:176431 and the race HUD
sub_402210 at :135496. Both feed it to sub_4E1B70, which strcmpi's address 0x2C.
A malformed catalog is refused whole, nothing is published and LOG_ERROR fires.
The client never divides to get the theme, it only multiplies (4 * step), so the
relation is ours to keep. row < 4 is enforced by fixed 4 element widget arrays,
not by arithmetic, and sub_4521C0 counts rows unbounded, so a fifth row in a
theme walks past those arrays into adjacent object memory.
THEME STEP GATE is server side only, and that is proven not assumed. The four
quest_step1_.._step4_ buttons are one radio group at QuestMenu +344236, built by
sub_44BF30 with exactly three sprites (00 normal, 01 hover, 02 selected). Every
method of that widget class was opened and every write to its per element state
field enumerated: the value set is {0 empty, 1 normal, 2 hover, 3 selected}.
There is no fourth value, no enabled byte, and no lock art (UI_Quest_locked.png
belongs to ScenarioMenu sub_437FB0, quest_rock_00.png has zero references).
sub_44C1A0 accepts a click on any step that is not already selected, and
sub_43CA50 applies it with no catalog or state test. So the client CANNOT lock a
step. Withholding the 0xFB rows for a theme is the only gate that exists: the
draw loop is bounded by sub_4521C0, which returns 0 for an unpublished theme, so
the list draws empty, nothing is hoverable, the selected index stays -1 and
sub_43CB40 never fires the C2S accept.
```

## server/game/include/handlers/SocialHandler.h

Line 1

```
@file SocialHandler.h
@brief messenger domain: chat, whisper, small talk, friends, requests,
blocks, presence, notes, lobby user list, profiles, room invites
Opcodes 0x6B 0x70 0x71 0x72 0x73 0x74 0x7A 0x7B used to be routed to the shop
purchase handler, so every friend row click granted an item. They are all
messenger verbs, proven at KnC-new.exe.c:210852 where the scrolling list over
dword_1A5CA48 (blocks) and dword_1A5BC30 (requests) fires sub_4820F0 /
sub_482050 / sub_482230.
Every reply is built by SocialPackets. Two polarity traps live there and are
respected here: 0x007A uses zero as the SUCCESS code (inverted versus 0x006F)
and 0x006F carries a message code from the sub_465FC0 table.
ACCEPT VERSUS REJECT IS NOW PROVEN, not button geometry. The pending request
row widget at KnC-new.exe.c:209891 registers button 0 with the sprite
"Popup/Messenger/Messenger_Tab_Aceiter_" (Aceitar, accept) and button 1 with
"Popup/Messenger/Messenger_Tab_Recusar_" (Recusar, reject). The click
dispatch at 210890 sends button 0 through sub_4820F0 which builds opcode 112
and button 1 through sub_482050 which builds opcode 113. So 0x0070 is ACCEPT
and 0x0071 is REJECT. Both PNG sets ship in Data/Eng/Image/Popup/Messenger.
HARD CLIENT HAZARD: S2C 0x0073 (sub_47B570) and S2C 0x0077 (sub_47B340) BOTH
look each id up with sub_44F050, which returns 0 on a miss and is then
written through. A status row for a player who is not already in that
client's friend container null-derefs the client. This class therefore
mirrors, per session, exactly which friend ids it has published in
0x0076 / 0x006F result 1 and never polls outside that mirror.
FRIEND RECORD, container dword_1A5AAF8 == unk_848648 + this, 11 dwords per
row (0x2C), capacity 100 (sub_44EF90 returns -1 past it), count at +0x1134:
+0x00 id +0x04 name 28 bytes +0x20 level +0x24 statusA +0x28 statusB
statusA (+0x24) has exactly two read sites, the same row draw loop emitted
twice (KnC-new.exe.c:211421 and :211493). Both are only "if (v < 0)", which
greys the row. -2 is the value sub_47B570 resets omitted rows to.
statusB (+0x28) is READ NOWHERE, so its encoding is ours to choose. Searches
run for that absence: byte offset literal 40, dword index 10, the absolute
field addresses, and every call site of every accessor on this container
(sub_44F030 by index, sub_44F050 by key, sub_44EF90 append, sub_44EF70 and
sub_44F090 clear, sub_44F0E0 erase) under BOTH spellings of the base. No
record pointer is ever handed to another function, so that set is closed.
EngineDLL.dll.c never touches the container. Do not trust a writer count
here: besides the literal +40 stores in sub_47B570 and sub_47B340, the
qmemcpy of 0x2C bytes in sub_44EF90 copies bytes 40..43 straight off the
0x0076 and 0x006F wire rows, and that path carries no offset literal at all.
CHAT lives here and not in ChatHandler because the client owns no whisper or
team opcode. sub_480990 puts the whole typed line on C2S 0x00B4 including the
"/w name " and "/t " prefixes it composed itself, so the server does the
split. The real whisper send is C2S 0x00B5 (sub_480C00) and it carries a
player id, never a name.
SMALL TALK is a two step handshake because the client has only one entry
point into its window. sub_475CE0 LOADS the box (chat_back.png plus close and
arrow buttons) and sets the created flag at +39827, which nothing else ever
writes. It has exactly one code xref and no data xref, so it sits in no
vtable: the caller is the 0x475E20 chunk of sub_4764E0, the accept button of
the S2C 0x007D popup. The requester therefore has to receive an S2C 0x007D of
its own once the target accepts, or its window never opens. S2C 0x7E 0x7F
0x80 are all nullsub_1 in the dispatcher, so none of them can open it either.
Do not confuse sub_475CE0 with sub_475A00 / sub_475B60, which are its hide
and free routines.
The TEXT leg of small talk is not wired in this build. While the box is up
byte_11FC19C is 1, so sub_4535F0 returns 27 and every keydown is routed to
sub_453A80 case 27 -> sub_475B90, which composes at most a local echo and
never calls the C2S 0x00B4 sender sub_480990. relaySmallTalk therefore cannot
fire for a stock client, and it logs a warning if it ever does.
```

Line 160

```
@brief C2S 0x6D answer to an S2C 0x6C invite, replyKey is not the room id
sub_47B030 is the S2C 0x6C handler and it alone picks the code. These are
every sub_481D70 call site in the image, so the set is complete:
1 sub_45C1E0, the popup no button, hides the box then answers
4 byte_11FC19C set, small talk window, armed by sub_4764E0
5 byte_1185FAC set, postcard note popup, armed by sub_46FD20
6 dword_B2360C is 11 or 15, the in race stages
7 dword_B2360C is 9 and the invite room id equals dword_BCE1B0
Code 5 caveat, this was refuted once already. Neither byte_1185FAC nor
byte_117FEE4 is ever written under its own name, both are *(obj+4) on a
parameter pointer, so follow the object bases byte_1185FA8 / byte_117FEE0
instead. Doing that: sub_46FD20 sets *(this+4)=1 on byte_1185FA8 and
sub_46FC10 is its loader (UI_popup_note_back.png). The byte_117FEE0 class
has NO write of 1 to +4 or to its created flag +24752 anywhere, so
byte_117FEE4 is dead-always-zero and can never raise code 5. sub_46FA90
and sub_470030 are TEARDOWN routines for these two, not loaders.
Accept is not a 0x6D at all. sub_45C130 is the yes button and it joins on
C2S 0x2F through sub_480F30, after an optional server hop.
Shipped Eng text confirms the key pairing: MSG_REJECT_GAME is "Player is
in the game", MSG_REJECT_SAME_ROOM is "Player is in same game room", and
both busy keys are "Player is busy". Code 1 has no key in the table.
```

## server/game/include/handlers/StartGridLoader.h

Line 1

```
@file StartGridLoader.h
@brief Track data service. Owns the S2C 0xC4 / 0xC3 catalog burst and every
shipped world file the server needs to grid, judge and rescue a car.
WHY IT EXISTS. SpawnPackets is a wire layer plus raw readers. Nothing there
decides which track id maps to which folder, nothing caches, nothing survives
a server that was deployed WITHOUT the client Data tree next to it. This class
is that decision layer and it is the only place the two data sources meet:
track_catalog / theme_catalog authored rows, become 0xC3 / 0xC4
DevClient/Data/Public/World/.. shipped start.ini follow_NN.ini warp.ini
regen.ini itembox.ini itembite.ini track.COL
track_spawn 604 transcribed start.ini rows, the fallback
when the World tree is not deployed
TRACK AND THEME IDS ARE NOT OURS TO INVENT. The client keys per track gimmick
loading on the track id (sub_4D4180 switches on track_rec+4) and per theme BGM
on the theme id (sub_4870B0 switches on theme_rec+4). Publish the wrong number
and either the gimmick .nif lookup fails, which makes sub_4875C0 return 0 and
pops "Track initialize fail !", or the wrong music plays.
BGM. sub_4870B0 switches on theme_rec+4 and calls sub_448890 with a sound
index. The name table is at 0x005CF4E8 stride 0x114, so the indices resolve:
theme 10 Forest case 0xA -> 0xF thema_forest_bgm_02 else default
theme 20 Cookie case 0x14 -> 9 or 10 thema_cookie_bgm_01 / _02
theme 30 Desert case 0x1E -> 0xB thema_desert_bgm_01
theme 40 Toy case 0x28 -> 0x11 thema_toy_bgm_01
theme 50 Devil case 0x32 -> 8 thema_lava_bgm
theme 20000000 case 0x01312D00 -> 7 thema_swamp_bgm
anything else -> 0xE thema_forest_bgm_01
There is NO case for 0x01C9C380, so theme 30000000 lands on the same default
as themes 60 70 80 90 and 10000000. Absence of a BGM case is not evidence
against a theme id.
Proven track ids:
track 11 Forest_02 sub_4DBBE0 Gimmick/mushman
track 12 Forest_03 sub_4DF9E0 Gimmick/tree_fairy_%02d
track 13 Forest_04 sub_4DF460 Gimmick/tree_door_%02d
track 20 Cookie_01 sub_4D4780 Gimmick/Ant_%02d
track 21 Cookie_02 sub_4D59F0 Gimmick/cookieman
track 22 Cookie_03 sub_4D4DB0 Gimmick/Chef_%02d
track 31 Desert_02 sub_4DCAF0 Gimmick/scorpion_%02d
track 40 Toy_01 sub_4DEDB0 Gimmick/toybox_%02d
track 56 Devil_07 sub_4D7C40 Gimmick/lavaman_%02d
track 60 Snow_01 sub_4DD150 Gimmick/Sheep_%02d
track 70 Palace_01 sub_4D53E0 Gimmick/cobra
track 80 Swamp_01 sub_4DDA90 Gimmick/swa_spider
track 82 Swamp_03 sub_4DDA90 Gimmick/swa_spider
so track_id == theme_id + (folder suffix - 1) and theme_id is a multiple of 10.
THE BAKED CATALOG IN .data IS DEAD, IT IS NOT A CLIENT DEFAULT. KnC.exe holds
12 theme rows at 0x005E1B20 stride 0x4C (field0, theme_id, char[33] folder,
char[35] key) and 38 track rows at 0x005E1EB0 stride 0x8C. NOTHING in the
image reads either one. Five search forms, all empty:
get_xrefs_to 0x005E1B20 / 0x005E1EB0 none
operand scan of 482747 insns for 0x5e1b2 0x5e1e 0x5e32 zero hits
byte search 201b5e00 b01e5e00 60325e00 ec325e00 zero hits
( 0x5e1b00 and 0x5e1b18 hits are unrelated objects )
whole file 4 byte scan for ANY value inside either row range, so a hoisted
base or a mid record pointer could not hide zero hits
The 12 theme rows were read straight out of KnC.exe at file offset 0x1E1B20
and they are intact: 10 Forest THEME_FOREST_INFO through 90 Race
THEME_RACE_INFO, then 10000000 Random, 20000000 Rally THEME_RALLY_INFO,
30000000 Battle THEME_BATTLE_INFO. The 38 track rows at 0x1E1EB0 carry ids
and floats only, both folder strings are empty in every row, and the rows for
20000000 and 30000000 both hold lap count 1 at rec+0x50.
The containers the client really consults are the zero filled 0x01A45F50
(tracks) and 0x01A4A558 (themes). sub_4875C0 loads exactly those two with
MOV ECX,0x1a45f50 and MOV ECX,0x1a4a558 and they fill only from S2C 0xC3 and
S2C 0xC4. So 0x005E1B20 is a dev era snapshot and never a fallback.
What DOES survive from it is the field layout, which the live readers confirm
on their own, plus the id / folder / key triples the developers intended:
theme_rec +4 id sub_4870B0 sub_487230 sub_487250
theme_rec +8 folder sub_4875C0 sprintf "World/%s/%s/track"
theme_rec +0x29 key sub_439630 sub_458550 sub_4751D0 -> sub_4E1B70
10 Forest 20 Cookie 30 Desert 40 Toy 50 Devil
60 Snow 70 Palace 80 Swamp 90 Race
10000000 Random 20000000 Rally 30000000 Battle
DevClient/Define/Eng/def_trans_index.txt ships ten THEME_*_INFO keys (COOKIE
DESERT DEVIL FOREST PALACE RACE RANDOM SNOW SWAMP TOY) and neither
THEME_RALLY_INFO nor THEME_BATTLE_INFO is one of them.
HOW TO GET THE THEME ID OF A FOLDER WHEN NO BGM CASE AND NO GIMMICK CASE
NAMES IT, which is the situation for Snow Palace Swamp and Race. Do NOT
infer from the arithmetic. Read the three shipped artefacts that agree:
1 the dead snapshot rows at file offset 0x1E1B20, stride 0x4C, which carry
the id, the folder and the loc key together
2 the THEME_<NAME>_INFO row in Define/Eng/def_trans_index.txt, which must
equal that key
3 the tile Data/Public/Image/Popup/SelectTrack/thema_<folder>.png, since
sub_443170 builds that name from theme_rec+8
All three line up for 10..90. Theme 90 Race is therefore not a guess even
though its BGM falls through to the default arm and it has no gimmick case.
NO SHIPPED FOLDER OWNS 20000000 OR 30000000, that is the whole answer. The
World tree holds exactly thirteen entries, identical on disk and inside
pak001.dat whose 21044 path strings were enumerated:
Cookie Desert Devil Forest License Light.nif Mission Palace Race Room
Snow Swamp Toy
Nine of them are the ordinary themes 10..90. The other three never reach a
theme row because each has its own hardcoded loader, and ONLY sub_4875C0
writes the theme rec at obj+78664 that sub_487230 and sub_487250 test:
License sub_487A90 "World/License/License_%.2d/..." rec at obj+78672
Mission sub_488000 "World/Mission/%s/..." rec at obj+78676
Room sub_488300 "World/Room/Sky|Floor|BgObj/%s" no rec at all
So Rally and Battle are dev era names for worlds this build never shipped.
Whatever folder a reserved row carries is a SERVER RETARGET and it must name
a tree that exists, else sub_4875C0 pops "Track initialize fail !".
Rally was nonetheless a real mode. Popup/RallyRule/RallyHelp.png ships and
its baked rules read: take the key item, the checkpoints arm, pass them all
in order, first home wins. That is exactly the sub_4A3BD0 rally branch, and
the dead track row for 20000000 carries lap count 1. The rest of the mode art
ships too, Lobby_Cha_Rally, Lobby_Quick_Rally, Panel/LapTime/Rally_wait,
Panel/Result/Result_rally and Room/WaitingRoom_Top_Rally. Only the WORLD is
gone: sub_4D4180 gives track id 20000000 its own gimmick arm, sub_4E0B40 with
the obfuscated path "World/%s/%s/Gimmick/twister" plus the Desert scorpion
sub_4DCAF0, and no twister asset exists on disk or in the pak while scorpion
does. Battle went the other way, its tile thema_battle.png and its track tile
track_battle_01.png both ship and only the world is missing.
RESERVED THEME IDS, never give one to an ordinary theme:
10000000 Random, drawn by the make room strip, dropped by
sub_439040 sub_439630 sub_458550 sub_453D50
20000000 Rally, dropped by sub_474A30 sub_4751D0 and by the same four,
so no strip ever draws or picks it
30000000 Battle, the ONLY theme sub_4751D0 draws when dword_BCE210 == 4,
and sub_410CB0 resolves a track under it through sub_453140
sub_487230 and sub_487250 are READ ONLY PREDICATES, "is theme 20000000" and
"is theme 30000000". Neither turns anything off by itself. Proven users:
sub_4818A0 and sub_47FD30 swap the movement packet body for Rally, sub_4B0570
clamps the item roll to 10 and 18 for Battle, sub_4A0970 skips the wrong way
state machine for both.
THE 0xC4 FOLDER OF A RESERVED THEME IS STILL READ, do not assume otherwise.
sub_443170 walks EVERY theme row with no reserved test and loads
"Popup/SelectTrack/thema_%s.png" from theme_rec+8. sub_474A30 does the same
for every theme except 20000000. sub_4875C0 builds "World/%s/%s/track" from
theme_rec+8 for ANY theme handed to it. A missing tile is harmless because
sub_441720 only returns 0, but a missing world folder is fatal and pops
"Track initialize fail !". So a reserved theme row must aim at a World folder
that actually ships.
NO PICKER FILTER PROTECTS sub_4875C0. It has exactly THREE callers, found by
scanning every E8 rel32 in .text, and not one of them tests a theme id:
stage 11 race sub_4028C0 track id at obj+92364
stage 15 ghost sub_425CB0 track id at obj+160, which is dword_C1A9C8
stage 17 quest sub_42D690 track id at obj+0x1EC, which is dword_C70A4C
That list is complete: a whole file search for the 4 byte constant 004875C0
returns nothing, so its address is never taken and there is no vtable slot
and no function pointer table to hide a fourth caller in.
The strip filters only decide what the PLAYER may click. Any track id the
server publishes reaches sub_4875C0 whatever its theme, so the folder of a
reserved theme is never merely cosmetic.
THE CLIENT MISSION STAGE uses no theme row. sub_488000 hardcodes the literal
"Mission" in every path it builds and takes the second component from
mission_rec+0x38 through sub_450CA0, so no theme id is consulted there and
20000000 is not the client Mission class.
That does NOT forbid pointing theme 20000000 at the folder "Mission", which
is what our theme_catalog seed does. Such a row travels the ordinary
sub_4875C0 path, "World/Mission/Mission_01/track" resolves because that tree
ships, and the rally semantics that ride on sub_487230 are what a lapless
mission wants. It is a deliberate retarget of a dead id, not a discovery, and
the rally UI it turns on is still the shipped Rally art.
ORDERING. Every 0xC4 row must land before the 0xC3 rows that name it and both
must land before the client enters stage 11, because sub_4875C0 resolves
track_rec = sub_4531F0(track table, track_id) then
theme_rec = sub_452FB0(theme table, track_rec+8) and hard fails on either miss.
```

Line 1, addendum 2026-09-14, cross ref docs/reverse/physics/INPUT_AND_STATS.md,
addresses read on KnC.exe.raw

```
0xC3 TRACKDEFINITION FIELD LAYOUT, handler FUN_0047F990, read order strict:
+0x00 u32 unknown
+0x04 u32 track_id, lookup key
+0x08 u32 theme_id, joins a 0xC4 row
+0x0C cstr folder_name, 36 byte slot, 2nd %s of World/%s/%s/track
+0x30 f32 tuning float 1  -> DAT_005EB6F0
+0x34 f32 tuning float 2  -> DAT_005EB6F4
+0x38 f32 tuning float 3  -> DAT_005EB6F8
+0x3C u32 unknown
+0x40 u32 fall_off_timeout_ms, client default 500
+0x44 u32 unknown
+0x48 u32 required license grade, byte_1A20B08 gate, "+72 grade" in PACKET_REGISTRY.md
+0x4C u32 unknown
+0x50 u32 lap_count, clamped 1..9, "+80 clamped 1..9" in PACKET_REGISTRY.md
+0x54..+0x64 five u32 unknown
+0x68 cstr tail_string, 36 byte slot, "%s_INFO" UI label
size 68 bytes fixed plus the two cstr lengths
world_track_init (0x4875C0) copies rec+0x30/+0x34/+0x38 straight into
DAT_005EB6F0/F4/F8. Meaning of the three, read live in KnC.exe.raw:
DAT_005EB6F0 = 0.4, ALSO body_finalize_wheels engine force prep value and the
  same value as car+0x32E8 steering scale, guess: per track engine/steering scale
DAT_005EB6F4 = 0.6, car_physics_tick_local engine force durability penalty scale,
  (1-durability)*0.6*0.16*engineForceBase, guess: per track engine power scale
DAT_005EB6F8 = 90.0, turn force baseline additive term, turnForce = roll*0.3+90.0,
  guess: per track baseline turning force
These three plus fall_off_timeout_ms are the only per track values in this
record the physics tick actually reads, the rest (grade, laps, names) are
menu/rules data, not tuning.
OUR SERVER fills the record from table track_catalog (created in
server/scripts/012_wire_all.sql, the two columns below added by ALTER TABLE in
013_kart_part_wire.sql lines 214 to 223, renamed by 056_column_renames.sql,
sent by SpawnPackets::trackCatalogEntry): columns tuning_engine_setup_bits and
tuning_engine_force_bits (raw uint32 float bit patterns, default
1053609165 / 1058642330, which ARE the bits of 0.4 and 0.6) feed +0x30 / +0x34
verbatim. Migration 013 also sets tracks 1 and 2 to 50 and 100 raw, which read as
floats near zero, origin of those two values not documented. Column tuning_turn_force
(a real FLOAT, default 90) feeds +0x38 through floatBits() at send time. Two more
FLOAT columns existed in the original 012 schema at default 0.4 / 0.6 and were read
into the row struct, but trackCatalogEntry never wrote them to the wire, they were
dead columns, the values that actually ship come from tuning_engine_setup_bits and
tuning_engine_force_bits instead, so 056_column_renames.sql drops them. Column
fall_off_timeout_ms, default 500, feeds +0x40 directly. The pre rename column
names for the first two tuning floats (see migration 013's own text, left unedited)
did not match their content (they are not fog), matching the note already in
PACKET_REGISTRY.md 0x00C3's field table ("camera_a .. NOT fog, see GAPS"). Migrations
012 and 013 are left untouched per the project rule against editing an old one,
056_column_renames.sql is additive only, the raw bits on the wire do not change.
```

Line 238

```
@brief theme is 20000000, the ONLY id sub_487230 matches.
sub_4A3BD0 takes its rally branch on that predicate alone. That branch
never reaches sub_4810D0, so no C2S 0x41 arrives, and its C2S 0x67 score
comes from sub_4A0780 which is straight distance plus 10000 times the
waypoint index. Theme 30000000 is NOT covered, it walks the normal
checkpoint path and does send C2S 0x41.
```

## server/game/include/packets/PacketBuilder.h

Line 176

```
lobby room list, the paginated grid built by sub_408020
entries live at lobby+0x6820, 512 slots of 0x88, gate at lobby+0x17828
0x2D sub_479630 -> sub_408360 add one room
int32 roomId, wstring name, then seven int32 landing at
+0x5C cur, +0x60 max, +0x68 icon, +0x6C locked, +0x70 channel,
+0x74 state, +0x78 time budget
channel MUST be zero, sub_408EB0 returns 0 flat and sub_407EA0 skips
every row whose channel field does not equal it
icon has to sit in 0..4 or sub_407EA0 skips the row as well
```

Line 357

```
=========================================================================
GAME MESSAGES (CMD 206) - IDA VERIFIED sub_47D250
=========================================================================
```

Line 532

```
0x119 (281) - Entity with strings - sub_47E800
Format: 2×int32 + 3×wstring
```

## server/game/include/packets/gen/CharCreatePackets.h

Line 1

```
@file CharCreatePackets.h
@brief login exchange plus character creation plus post creation transitions
Wire spec source: KnC.exe 2014.10.24 client version string "178".
Handlers referenced: sub_478DA0 (0x01), sub_478D40 (0x02), sub_479220 (0x03),
sub_479230 (0x04), sub_479020 (0x07), sub_479080 (0xA7), sub_47D3B0 (0x0A),
sub_4793F0 (0x0E), sub_479340 (0x19), sub_478B50 (0xBE).
Builders referenced: sub_480430 (C2S 0x07), sub_480500 (C2S 0xA7),
sub_4805D0 (C2S 0x04), sub_4806F0 (C2S 0x18).
```

Line 24

```
@brief login + character creation packet family
All builders return a Packet whose payload is already complete. Every fixed
size packet is size checked at runtime and logs to the PACKET tag on
mismatch. A short frame throws the client side C++ "Invalid Read" object and
sub_476CC0 has no catch, so a wrong size is a hard client death.
```

Line 132

```
-----------------------------------------------------------------------
C2S 0x0018 requested_stage values, the sub_404410 stage table
-----------------------------------------------------------------------
```

## server/game/include/packets/gen/CustomCarPackets.h

Line 1

```
@file CustomCarPackets.h
@brief Custom car and car craft wire builders CarFactory stage 18
S2C 0x0107 preset list, 0x0108 part definition, 0x0109 part instance,
0x010A open ack, 0x010B save result.
C2S 0x010B save request.
Plus the custom_car[0x3C] block carried as the tail of 0x0021 and 0x003E.
Ordering the client actually enforces (sub_430EF0 snapshots all three
containers at stage init): every 0x0107 / 0x0108 / 0x0109 must be buffered
BEFORE 0x010A. Order among those three does not matter, completeness does.
Two proven crashes guarded here:
- 0x0107 with count 0 -> sub_430EF0 LABEL_146 and sub_42FBD0 deref null.
- a part_key on the wire with no matching 0x0108 def -> sub_430420,
sub_42F230 and sub_42FE20 all deref the def with no null check.
```

## server/game/include/packets/gen/DriftBoostPackets.h

Line 2

```
@file DriftBoostPackets.h
@brief Drift, boost and the speed model on the wire. KnC.exe 2014, IDA base 0x400000.
THE ONE FACT THAT SHAPES EVERYTHING: drift and boost are entirely client authority.
All 20 call sites of sub_496BE0 (the boost grant) live in input, physics and UI code.
NONE is inside a dispatcher handler, so no S2C packet can start a boost on a client.
sub_49AA90 (drift entry) sends nothing. There is no C2S packet for drift start, drift
end, mini turbo charge or mini turbo fire.
The server therefore has exactly two jobs here:
1. relay C2S 0x40 so remote clients play the right drift and boost fx
2. observe the state word and flag what is physically impossible
WHERE THE SIGNAL LIVES. Only the u16 state word at C2S 0x40 payload offset 17 carries
drift and boost. Sent every 100 ms while the race state global is 11 or 9. The map is
verified twice, against Ghidra FUN_0049beb0 0x49bfb4..0x49c0aa (encoder) and against the
decoder sub_49ED90 at KnC-new.exe.c:245954..246006.
b15..12 steer index b11..8 engine rpm index
b7 boost class 0 (mini turbo) b6 boost class 1 (item) b5 reverse gear
b4 drifting, DIRECTION IS LOST b3 mini turbo charged
b2 steer input left b1 steer input right b0 never set
LOAD BEARING NEGATIVE RESULTS, do not waste a debug session rediscovering these:
- S2C 0x47 has NO case 0 and NO case 1. Relaying a boost item use there draws nothing
on the receiving client. Remote boost fx come only from 0x40 bits 7 and 6.
- S2C 0x4B only branches on kind 10 and 16. Sending a boost level there is dropped.
- There is no 0..3 mini turbo LEVEL in this build. car+13824 is written to 0 and never
incremented anywhere, so both level gated grant paths are dead code. Charge is binary.
- sub_49FF90 is NOT a second wire decoder. It is the .rep replay codec on an
incompatible 28 byte record. Only sub_49ED90 decodes the wire.
WHERE THE PHYSICS NUMBERS COME FROM. Not a Define file, not the .car files, which have no
loader in either binary. The per kart engine, speed, drift and boost parameters are the
17 float block inside the S2C 0x00C0 kart definition catalog record, plus the per part
bonus block from S2C 0x0108. sub_490A70 memcpy's the 0xC0 record to car+13220 so the stat
block lands at car+13384 and every physics read resolves there.
@see MotionPackets for the transform codec, the S2C 0x40 broadcast and S2C 0x47.
This file never duplicates a vec3 packer or a broadcast builder.
```

Line 132

```
@brief S2C 0x00C0 kart definition record, sub_47F4F0.
WIRE OFFSETS ARE NOT RECORD OFFSETS. The handler reads field 3 as ONE byte
(sub_44E910(a2,&v11,1) at KnC-new.exe.c:224489) into an int slot, and the three
cstrings are strlen+1, so from field 4 onward the wire runs 3 bytes ahead of the
320 byte record that sub_44F510 memcpy's into the catalog. Emitting 4 bytes for
field 3 destroys every packet after it. Wire layout:
0x00 u32 unk00 no reader found
0x04 u32 unk04 no reader found
0x08 u32 kartId lookup key, sub_44F6F0 scans this+3 stepping 80 dwords
0x0C u8 unk0c ONE byte, record bytes 0x0D..0x0F stay stack garbage, unread
0x0D i32 classCode record+0x10 = car+13236, physics branch switch, load bearing
0x11 i32 partsEnabled record+0x14, gates all 7 part bonus applications on == 1
0x15 i32 unk18 no isolated reader
0x19 i32 unk1c no isolated reader
0x1D cstr modelName set match key, strcmp against part record+0x14
var cstr name2
var cstr name3
var 32B blob32
var 68B statBlock 17 LE floats, lands at car+13384
var 8B pair0
var 8B pair1
var i32 optionCount
var 16B option[count]
Fixed part is 149 bytes plus the three NUL terminated strings plus 16 per option.
```

Line 132, addendum 2026-09-14, cross ref docs/reverse/physics/INPUT_AND_STATS.md
and docs/reverse/CLIENT_PHYSICS_MAP.md, addresses read on KnC.exe.raw

```
THE 17 STAT INDICES. record+0xA4 (rec offset, the 68 byte statBlock above) lands
at car+0x3440 through stat_catalog_store (0x44F510, copies 320 bytes id..pair1
into the catalog slot) then car_apply_kart_loadout (0x490A70, copies the 320
byte slot into car+0x33A4). Proven tick meaning, base car+0x3440+i*4, bonus
car+0xA7940+i*4:
idx 3  max speed          clamp(stat+bonus+1, 1.0, 2.0) * DAT_005EB6FC (320.0)
idx 4  steering gain       feeds car+0x2974 drift steering gain
idx 5  mini turbo target   clamp((stat+bonus)*0.2+1.0, 1.0, 1.2) * 120.0 km/h
idx 7  turn force
idx 8  wheel spin          kind 2 vehicles read this directly for wheel torque
idx 9  wheel steer angle   kind 2 vehicles read this directly for front angle
idx 10 drift charge rate   clamped 0.3 to 0.8, quartered while slowed
idx 11 drift steer         into body_vec3_set 0x4ED3D0
idx 12 mini turbo threshold  gauge must pass stat * DAT_005EB704
idx 13 mini turbo hold time  hold must pass stat * DAT_005EB708
idx 14 grip
idx 0, 1, 2, 6, 15, 16 have no confirmed read site, open question.
OUR SERVER emits 0xC0 through two paths. PartStatPackets::loadKartDefs
(PartStatPackets.cpp lines 560 to 597) fills KartDefRow.stats from the single
stat_block_hex blob column of table def_kart_wire, the captured bytes verbatim.
CORRECTION 2026-09-14: an earlier version of this note said GameServer.cpp
lines 2443 to 2463 read table kart_catalog for these columns. Re-checked
against the live source, that is wrong. Those lines build stats from the
vehTemplates map (query is against vehicle_templates, columns stat_speed
stat_accel stat_handling stat_drift stat_boost), and every statCol() lookup
by the old kart_catalog stat names always misses that map and falls back to
its default, dead code, not a second reader of kart_catalog. The real
kart_catalog reader is DriftBoostPackets::loadKartCatalog /
DriftBoostPackets::loadKartDef, columns (renamed by
server/scripts/056_column_renames.sql, old names in parens) stat00
(stat0_torque_trim) stat01 (stat1_speed) stat02 (stat2_accel) stat03
(stat3_boost) stat04 (stat4_boost_pitch) stat05 (stat5_handling) stat06
(stat6_pitch_scale) stat07 (stat7_roll_scale) stat08 (stat8_drift) stat09
(stat9_steer) stat10 (stat10_drift_threshold) stat11 (stat11_drift_charge)
stat12 to stat16 unchanged, into a plain array sent through
DriftBoostPackets::kartDefinition. The old column names for indices 0, 1,
2, 6, 15, 16 and for 3, 4, 5, 8, 10, 11 encoded a GUESSED meaning that did
not match the proven tick meaning above, the new stat00..stat16 names carry
that proven meaning in a COMMENT instead, do not trust a bare index number
over the index table either.
NOT READ FROM .car. docs/reverse/physics/INPUT_AND_STATS.md searched
KnC.exe.raw for the strings ".car", "%s\.car" and "Data/Car" and got zero hits,
and no function in the traced call graph opens a matching path. The exe gets
these 17 numbers only from this wire packet, the shipped Data/Car/*.car files
are very likely server or tool side source data, not something the client
loads, and their field order does not provably match this 17 float block.
CORRECTION 2026-09-14 night. The .car files ARE read, the path string is built
from an obfuscated char array so the string search could not find it.
car_physics_setup 0x494D50 reads ./Data/Car/default.car and car_apply_kart_loadout
0x490A70 reads Data/Car/<kart name>.car through catalogue_read_file 0x4EFF20 into
the spawn catalogue car+0x2EA0 (chassis, wheels, gears, torque curve, springs).
The 17 stats above still come only from this packet, the two blocks are unrelated.
Full layout in docs/reverse/physics/RIGID_BODY.md under Catalogue.
CORRECTION 2026-09-14, second pass, same day the two paths above were audited.
Commit bbb26f50 deleted PartStatHandler, the only caller of
PartStatPackets::loadKartDefs and PartStatPackets::kartDef, and grep across
server/game confirms DriftBoostPackets::loadKartCatalog and
DriftBoostPackets::kartDefinition had zero callers of their own even before
that commit. Both paths above were already dead when this note was written,
not a live two path emit. Both are now deleted along with the deleted path's
own loadKartCatalog. THE ONLY 0xC0 SENDER TODAY is PacketBuilder::vehicleCatalog,
called from the login burst in GameServer.cpp against table vehicle_templates,
through the shared server/game/src/packets/KartDefinitionWire.cpp
writeKartDefinitionBody. Its 17 stats come from vehTemplates statCol(), the
same dead default fallback path this note already flagged above, not from
kart_catalog stat00..stat16. DriftBoostPackets::loadKartDef (singular) survives
only because RaceHandler::loadTuning still reads it through
DriftBoostPackets::loadEffectiveStats for server side physics tuning, a
different consumer than the 0xC0 packet.
```

Line 314

```
---------------------------------------------------------------------
state word masks, encoder sub_49BEB0 decoder sub_49ED90
---------------------------------------------------------------------
```

## server/game/include/packets/gen/GachaPetPackets.h

Line 1

```
@file GachaPetPackets.h
@brief Gacha and pet domain: roll parse, roll result, pet catalog, pet equip glue.
Wire facts (KnC.exe reverse sweep plus adversarial review):
C2S 0x00ED sub_4830C0 gacha roll, 28 bytes, the WHOLE owned ticket row
S2C 0x00ED sub_47D5E0 gacha result, 20 byte header + 28 byte ticket echo + typed tail
S2C 0x0135 sub_47EEF0 level up random item popup (dispatcher case 309)
S2C 0x0103 sub_4800D0 pet definition catalog, cap 32
S2C 0x0104 sub_47E3A0 owned pet list, FULL REPLACE, cap 64
C2S/S2C 0x00B7 / 0x00B9 / 0x00BA buy / equip / unequip, pet leg is category 4
OPCODE CORRECTION: the level up reward popup is 0x0135 (309), NOT 0x0134 (308).
The dispatcher sub_4777C0 has no case 308 at all and drops it silently.
CARCRAFT TAIL ORDER TRAP, do not share one serializer:
S2C 0x00ED cat 5 and S2C 0x0135 cat 6 -> blob[0x84], u8 has_slot, [blob[0x34]]
S2C 0x00B7 cat 6 -> u8 has_slot, blob[0x84], [blob[0x34]]
This file only ever emits the first order.
NO PET PRIZE EXISTS. The gacha switch is 0=char 1=kart 2=item 3=part 4=roomcraft
5=carcraft, and the 0x0135 switch is the 0x00B7 enum with case 4 (pet) deliberately
missing. A pet can never be delivered through either popup.
RACE EFFECT is entirely client side. It is armed only by the 0x0104 list sent at
lobby time (first row with rec+0x08 == 1, key mapped through the literal switch on
10 / 20 / 30 / 40). Nothing on the wire during a race can change it.
CRASH NOTES the caller must respect:
- 0x00B9 and 0x00BA cat 4 resolve the record by INSTANCE UID with no null check,
an unknown uid writes to address 0x00000008. Use ackRowIsSafe first.
- a 0x0103 row whose str1 has no folder under Data/Public/Pet/Body makes
sub_48CBB0 return 0 and the driver binds to entity slot 0.
- a category 2 gacha prize whose base key equals the ticket base key lands in the
SAME 0x001D container and wipes the ticket row.
```

## server/game/include/packets/gen/MotionPackets.h

Line 72

```
@brief Motion sync wire layer. Dispatcher sub_4777C0, car table byte_1B19090.
Covers S2C 0x40 sub_47FD30 broadcast, C2S 0x40 sub_4818A0 self report at a fixed 10 Hz,
S2C 0x68 sub_47AE30 hard teleport, plus the neighbouring 0x47 0x69 0x6A 0xA5 that touch
the same interpolation slot.
FRAMING. The client re frames from the 8 byte header on every iteration of sub_476CC0
and advances by payload_len regardless of how much a handler consumed, so a short read
is harmless and an unknown player id never desyncs the stream. The only real failure is
a payload SHORTER than the handler reads, which throws out of the dispatcher, and a
payload plus header of 0x2000 or more, which drops the connection with error -4. Every
builder here is exact size and asserts it at runtime.
TARGETING. sub_48DEA0 matches on the id at car+1860 AND requires the active byte at
car+1856 to be exactly 1, so a car must already be spawned through the room and join
path before any motion opcode reaches it. Entries for unknown ids are silently dropped.
SELF EXCLUSION. The local car is routed to sub_49C0D0 which never pops the 0x40 queue,
so a recipient must never see its own id in a 0x40 broadcast, it only wastes a slot.
LATEST WINS. The queue at car+13128 holds exactly one entry, so two 0x40 packets inside
one render frame means the first sample is discarded.
FIRST SAMPLE SNAPS. Until a car has taken its first sample both step divisors are 1.0,
so the very first 0x40 teleports the remote car rather than easing it.
rawWorld selects the uncompressed 28 byte transform. It is true only when the active
mode descriptor dword equals 20000000, checked by sub_487230 on BOTH sides, so client
and server always agree. Every ordinary track is compressed.
```

Line 169

```
------------------------------------------------------------------
state bits, encoder sub_49BEB0 decoder sub_49ED90
------------------------------------------------------------------
```

## server/game/include/packets/gen/PartStatPackets.h

Line 1

```
@file PartStatPackets.h
@brief Part and accessory definition catalogs plus the kart stat model.
Wire facts re-derived read by read from the decompiled handlers, not guessed:
S2C 0x00C0 sub_47F4F0 kart definition, CARRIES THE 17 FLOAT BASE STAT BLOCK
S2C 0x00C1 sub_47F6B0 item / accessory definition, NO stat block
S2C 0x00C2 sub_47F800 kart part definition, NO stat block
C2S 0x00CC sub_482E60 / sub_482FE0 kart part use notify, 0x1C record + u8
Every one of these packets is VARIABLE LENGTH. The three name fields are
NUL terminated strings read by sub_44EB30 (strlen based) and the price list
is a u32 count followed by count * 0x10. Dumping a fixed size record image
desyncs the reader on the first row, so the builders here serialise field by
field in wire order, which is NOT record order for 0x00C0.
NOT BUILT HERE, already owned elsewhere, do not add a second builder:
0x00BE catalog reset and 0x00C6 price rows -> ShopPackets
0x0107..0x010B car craft preset / part def / part instance -> CustomCarPackets
0x001C 0x001D 0x001E owned lists and the 0x00B9 acks -> InventoryPackets
STAT MODEL, all proven against sub_48F710 (race) and sub_4286E0 (garage),
both of which run the identical arithmetic:
scale = grade / 50 signed C truncating division
k in 0..9 acc[k] += stats[k] * scale + stats[k]
k in 10..16 acc[k] += stats[k] grade ignored
Purely additive. No cap, no cross part multiplication, no diminishing return.
Seven slots are summed into one accumulator that is zeroed first.
SCHEME GATE: sub_490A70 only runs the accumulation when the kart definition
field at +0x14 equals 1. A kart whose scheme selector is not 1 races and
displays with an all zero bonus block no matter what is equipped.
TIRE EXTRAS: the three floats at part definition +0xCC reach the physics only
from the TIRES slot and they bypass the grade multiplier entirely.
GARAGE BARS: there are exactly FOUR bars, not seven. They are computed by
sub_428AB0 by normalising against the min and max of the WHOLE kart catalog
the server sent, so adding one kart row silently moves every other kart bar.
```

## server/game/include/packets/gen/QuestPackets.h

Line 13

```
@brief quest domain packets, client stage 26 QuestMenu
S2C 0xFB quest definition, 0xFC quest state list, 0xFE accept ack,
0x100 discard ack, 0x101 complete, 0x102 progress.
C2S 0xFE accept, 0x100 discard, 0x102 progress report.
Protocol.h calls 0xFB S_BUDDY_LIST_ENTRY and 0xFC S_BUDDY_STATUS_LIST and calls
0x100..0x102 S_ENTITY_DATA_*. All of those labels are wrong. Local constants are
used in the cpp, do not wire the Protocol.h names.
Two containers on the client:
catalog dword_1A66738, 50 records of 112 bytes, count at +0x15E4, appended by 0xFB
state dword_1A67D20, 50 records of 12 bytes, count at +0x25C, full replace by 0xFC
Stage 26 sends NO screen open opcode, the top bar switches stage locally, so there is
no screen ack to race. Both containers must already hold the data when the player can
first reach the button, so push the catalog and the state list at character enter.
Delivery order the client requires:
1. S2C 0xFB x N, one packet per definition, catalog only ever grows
2. S2C 0xFC once, it clears and replaces the whole state list
3. after that 0xFE 0x100 0x101 0x102 are deltas onto the state list
Crash constraints, all of them unchecked derefs in the client:
- quest_index MUST equal 4 * theme_id + row with row in 0..3 and the set contiguous
from row 0. sub_43CCC0 walks 4*theme .. 4*theme + count - 1 and feeds
sub_452240 result + 44 straight into the string table with no null test.
- at most 4 catalog rows per theme_id, the drawn button set is
24020 * (row + 4 * state) + this + 24176 and only 12 sets exist.
- state MUST be 0, 1 or 2. state 3 lands on the theme radio group at this+312416.
- every state row quest_index MUST exist in the catalog. The race HUD does
sub_452240(catalog, *sub_451F20(list,1)) then reads +44 with no null test.
- 0x101 and 0x102 quest_index MUST already be in the state list, sub_452110 returns
0 and the handler then writes absolute addresses 4 and 8.
```

## server/game/include/packets/gen/RoomCraftPackets.h

Line 1

```
@file RoomCraftPackets.h
@brief room craft domain RoomEditer stage 19
S2C 0x010C object catalog sub_47FF70
S2C 0x010D owned instance sub_47D9A0
S2C 0x010E stage push sub_47D9D0 empty payload
C2S 0x010F save request sub_483450
S2C 0x010F save ack sub_47DA00
plus the 48 byte decor record reused by the 0x0013 tail sub_47FC20 -> sub_488300
```

## server/game/include/packets/gen/SocialPackets.h

Line 1

```
@file SocialPackets.h
@brief chat 0x00B4, whisper 0x00B5 / 0x002A, system line 0x0126, lobby user
list 0x0132 / 0x0133, profile 0x0028 / 0x0072, friends 0x006F 0x0070
0x0071 0x0073 0x0074 0x0076 0x0077 0x0078, block 0x0079 0x007A 0x007B,
note 0x0081, smalltalk 0x007D..0x0080, room invite 0x006C / 0x006D
Layouts come from the KnC.exe 2014 social reverse sweep (dispatcher
sub_4777C0) after its adversarial review. Where the review disagreed with
the first pass the review wins; the affected packets are 0x006C (field
labels were swapped) and 0x0132 (the leading dword is a live vptr).
Fields with no proven reader are written as zero and are NOT exposed as
parameters. Never synthesize a value for them.
HARD CLIENT HAZARDS the caller must respect:
- the client reads UTF-16 (sub_44EB60) and ASCII (sub_44EB30) strings with
qmemcpy and NO destination bound. An overlong name or message on 0x00B4,
0x0126, 0x0025 or 0x0027 smashes the client stack, it does not truncate.
Every builder here clamps and shouts, but the caller should pre-clamp.
- 0x0073 and 0x0077 look each id up with sub_44F050, which returns 0 on a
miss, then write through it. A status row for a player who is NOT already
in that client's friend list null-derefs the client. Send status only for
ids that were in the last 0x0076 sent to that same session.
- container caps are 100 friends, 100 pending requests, 30 blocks. Past the
cap the client drops rows silently.
- 0x0132 is a FIXED 296 byte page, always 7 entry slots, entry_count 0..7.
A larger count walks the draw loop past the array.
- 0x00B5 needs both 0x4C8 UserInfo blocks in full even though this handler
reads only +0x00 and +0x486 out of them.
Nothing in the client requests 0x0076 / 0x0078 / 0x0079, the server must
push them. Their arrival time relative to the lobby ack is unconstrained,
the messenger re-reads the containers live every frame.
```

## server/game/include/packets/gen/SpawnPackets.h

Line 1

```
@file SpawnPackets.h
@brief Getting cars onto the track: grid placement, checkpoints, laps, respawn relay.
TRACK DATA IS CLIENT SIDE. The server never puts a spawn position on the wire for a
race start. It sends a track id (S2C 0x14, owned elsewhere), the catalog rows that name
the folder (S2C 0xC3 / 0xC4) and one grid_index per racer (S2C 0x3E). The client reads
<base>/start.ini off disk and places the car itself the moment it parses 0x3E.
PATH RESOLUTION (sub_4875C0): track_rec = sub_4531F0(track table, track_id);
theme_rec = sub_452FB0(theme table, track_rec+8);
base = "./Data/Public/World/<theme_rec+8>/<track_rec+12>/". Both catalog rows must have
been pushed BEFORE the client enters stage 11 or sub_4875C0 fails and no track loads.
COORDINATE SPACE. Z is up. start.ini space == C2S 0x40 wire space with no transform.
The physics and render space is (-x, -y, z), the server never sees it. sub_486C20 adds
+0.5 to z only, on the in range path, so the ini value is the ground contact.
YAW. Degrees in [0,360). Forward is (-cos A, sin A), i.e. the standard math angle equals
180 - A and A == 0 points down -X. sub_4EC290 multiplies by pi/180 and the 0x40 yaw byte
scale is 255/360, so the unit is degrees three ways over.
FINISH IS SERVER OWNED. Laps are counted client side but nothing in stage 11 compares
the count to a total, so the race only ends when the server sends S2C 0x3C. That packet
and the whole result table live in ResultsPackets, not here.
FRAMING. sub_476CC0 re frames from the 8 byte header every iteration and re inits the
reader per packet, so a handler that returns early never desyncs the stream. An unknown
player id in S2C 0x68 simply drops the teleport.
```

Line 61

```
dispatcher does DEC EAX before the table lookup so every ida S2C label reads
one low. sub_47FAE0 is the track row reader and it sits on REAL opcode 0xC5,
its commit sub_453270 is the neighbour of sub_453330 which the tutorial
stage uses to look a track up.
0xC3 not 0xC5. The dispatcher sends 0xC3 to FUN_0047F990 which appends to
the TRACK container at 0x01A45F50, and 0xC5 to FUN_0047FAE0 which is the
LICENCE TEST container. Every track row was landing in the wrong table, so
the track count stayed zero and the room draw faulted every frame on a null
catalog record. Measured live at 0x004109A3.
```

## server/game/src/GameServer.cpp

Line 865

```
0x6B 0x70 0x71 0x72 0x73 0x74 0x7A 0x7B ARE NOT SHOP OPCODES
proven at KnC-new exe c 210852 a seven row scrolling messenger list over
dword_1A5CA48 blocks and dword_1A5BC30 requests fires sub_4820F0 sub_482050
sub_482230 on a row click so every friend click used to run handlePurchase
real meaning see SocialPackets 0x70 accept 0x71 reject 0x72 profile by name
0x73 presence poll 0x74 friend del 0x7A block add 0x7B block del 0x6B no sender
unrouted for now they fall to the warn branch which never replies
gift buy sub_482880 must not echo 0x98 client reads 220 bytes else crash
```

Line 2863

```
NOT seating the player here. Measured in client memory: the 0x32 does set
slot zero enabled at membership+0x6A20, but sub_40D650 and sub_40CC90 both
bail on the gate at membership+0x1CDD8 which is only armed by sub_40F7E0
through the stage setter sub_404410, and the lobby never arms it, so a 0x21
at the lobby is dead weight on the burst.
0xD9 is what the lobby needs instead. sub_47D540 reads
i32 playerId | 0x2C char record | 0x38 kart record | 0x3C custom car
hands the four to sub_40CC90 for the model build, and then, ONLY when the
player id equals the local one at DAT_01A20658, copies char+0x00 into
DAT_01A20B18 and kart+0x00 into DAT_01A20B1C. Those two globals are the
client side selected character instance and selected kart instance, and
every model build reads them, sub_42D690 sub_425CB0 sub_43A320 sub_41B860
and the icon builder sub_4A5ED0. Until this packet arrives they are zero,
so the stand has no pair to build and comes up empty.
The room quick swap already sent this. The lobby never did.
```

## server/game/src/handlers/GhostHandler.cpp

Line 298

```
0xAD sub_47CAE0 does sub_4538B0 then sub_458510, and sub_458510 opens the
record board POPUP. On the stage 23 path that popup lands on top of the
real Ghost screen sub_439350 already built, two track lists and two record
panels stacked. The 0x011D handler sub_47E980 already does sub_4538B0
itself so the wait modal closes without it.
Only send 0xAD when the caller actually wants the standalone popup.
```

Line 677

```
sub_47DD10 writes 0xF3 rec +0x0C into dword_C70A4C which IS stage 17 obj+0x1EC
sub_42D690 reads obj+492 and hands it to sub_4875C0 so this row must match it
```

## server/game/src/handlers/ProgressionHandler.cpp

Line 25

```
ctxA and ctxB are NOT unread scratch, an earlier pass claimed that from a literal
search on dword_11B44E8 and dword_11B450C and it was wrong.
sub_47EEF0 stores them at 0x011B44E8 and 0x011B450C. Those two addresses are
byte_11ADBC8 + 26912 and + 26948, that is slot 8 of the two 8 slot reward reel
arrays sub_473390 fills at + 26880 and + 26916. The reel renderer reads
byte_11ADBC8[4 * idx + 26880] for the type and [4 * idx + 26916] for the key with
idx starting at 0 and incremented with NO clamp, so idx 8 lands exactly on these
two. Base relative like that, they carry no xref to the literal address, which is
why the single form search saw only writes.
Zero is still correct, for a different reason. Type 0 routes to sub_443320, which
resolves the key through sub_450FF0 on the 0x00C2 container and skips the draw on
a negative index, so key 0 draws nothing instead of faulting.
```

## server/game/src/handlers/RoomCraftHandler.cpp

Line 81

```
A room with no floor has no collision mesh. sub_488300 only calls the
"Room/Floor" loader on a category 1 record, that loader is the only thing that
reads track.COL, and with no COL the navmesh count at 0x01ADF5EC+8 stays zero,
so sub_00486300 returns 0, sub_00490a70 falls into its epilogue with AL zero
and the room answers "Set body fail #1". Measured live with the debugger.
The room world sub_488300 builds comes entirely from this list. Category 1 is
the floor and it is the one that loads the collision through sub_485580, and
without collision sub_486300 cannot place a car, so sub_490A70 fails and the
client shows "Set body fail #1". A sky rides along so the room is not black.
```

## server/game/src/handlers/SocialHandler.cpp

Line 99

```
client owns this one at 0x005A610C and its own guard in sub_480990 prints it
TODO unproven cosmetic: the shipped Eng text ends "after %d seconds" and only
sub_480990 fills that token (sub_4B4140). The 0x0126 path in sub_47EC00 does
swprintf(buf, L"%s", resolved) so a server copy shows a literal %d on screen.
Either ship a key with no token or accept the artifact. Not settled here.
```

Line 393

```
The level field is a zero based index into the icon sheet preloaded by
sub_4425F0 which starts at Icon/lv_icon_001, so a raw ten renders eleven.
sub_45A4A0 confirms it, the full bar test is level+1 >= 50 not level >= 50.
```

Line 1375

```
TODO unproven that this path can ever run. The stock client has no way to
SEND a small talk line: while byte_11FC19C is 1 sub_4535F0 returns 27, so
every keydown goes to sub_453A80 case 27 -> sub_475B90, which composes a
local echo, skips even that when online and bound, and never calls the
C2S 0x00B4 sender sub_480990. All three sub_480990 call sites (135122,
140025, 145845) sit on the other side of that guard. So the 0x7D/0x7E/0x80
lifecycle is complete but the text leg is not wired in this build. Kept
because it is wire correct, and the client may be patched to reach it.
```

## server/game/src/packets/PacketBuilder.cpp

Line 364

```
0x13 S_PLAYER_ROOM_DATA, handler sub_47FC20, it reads
int32 -> DAT_00BCE1B0
wstring -> DAT_00BCE1B4 room name, NUL terminated, never padded
int32 x8 -> BCE22C BCE210 BCE224 BCE214 BCE218 BCE21C BCE220 BCE244
sub_452380() clears the room object container
int32 count
count x 0x30 each one handed to sub_4523A0
Those 0x30 records are NOT the players. sub_4523A0 fills the container at
dword_BCE850 that sub_488300 walks to BUILD THE ROOM WORLD, entry+0x08 is
the category, 0 sky 1 floor 2 background 3 object 4 effect, and entry+0x2C
has to be 1 or the entry is skipped. The floor case is the one that loads
the collision through sub_485580.
We were writing the player list there. So the room world was never built,
DAT_01ADF7E4 stayed at zero sections, sub_486300 could not place the car,
sub_490A70 returned false and printed "Set body fail #1" from sub_409DA0.
No collision, no car, no chibi, in the waiting room and in the lobby stand
that shares the same build.
The players ride 0x21 sub_40D650, one packet each, which is already sent.
The record list belongs to RoomCraftHandler::appendRoomDecor, call it on
the returned packet.
```

## server/login/src/handlers/HandshakeHandler.cpp

Line 705

```
0x54 sub_47AA00 calls sub_4774C0 INSIDE the parse loop, so the reconnect
lands mid parse. The loop then subtracts the frame it just handled and the
buffered count goes to minus the frame size, then it arms a second recv on
top of the one the reconnect already posted. Two reads on one OVERLAPPED
and the stock client wedges. Measured 0 of 5 logins survive.
0x19 sub_479340 only hands the target to sub_405EB0 which stores it and
raises a pending flag, then sub_405EF0 does the reconnect from the main
loop, outside the parse. Same job, no double arm.
wire order differs, 0x19 is host then port then mode, 0x54 was flag then
host then port. mode 8 is the redirect handler literal, 4 makes the client
ignore the port and use its Network2 ini instead.
measured: 0x54 1 of 3 logins, 0x19 0 of 5. Neither fixes the stock client,
the race is in the client recv arm, see docs/packets/STOCK_CLIENT_LOGIN.md
```

## server/scripts/014_gacha.sql

Line 1

```
============================================================================
014_gacha.sql server side GACHA
Client facts this schema is shaped by, all read out of KnC.exe:
sub_4571D0 the popup resolves its ticket EVERY FRAME with a hardcoded
sub_4516D0(2000) on tab 0 and sub_4516D0(2001) on tab 1, so a
ticket is an owned_item row whose base_key is exactly 2000 or 2001
sub_4830C0 PLAY sends C2S 0xED only when row+0x14 active is non zero and
row+0x10 count is above zero, otherwise the button is dead
sub_47D5E0 the reply is 20 byte header + 28 byte ticket echo + a typed tail
picked by header dword 1, and dword 2 is the definition key the
reveal looks up in that category own catalog
prize_category is the 0xED enum 0 character 1 kart 2 item 3 part
period_mode is the sprite selector 0 permanent 1 days 2 times
WARNING this file DROPs gacha_items and gacha_history. Every row the emu needs
lives here, never insert prize rows only into the live DB or a rerun wipes them.
============================================================================
```

## shared/src/include/net/Protocol.h

Line 271

```
proven from the dispatcher dump plus the handler reads
0xBF sub_47F4F0 commits sub_44F510 the KART container
0xC0 sub_47F6B0 commits sub_450750 the CHARACTER container
the two were swapped which left the lobby with no driver and no kart
```

Line 279

```
0xC1 handler sub_47F800 commits sub_450F40 into the container sub_4510C0
searches. The kart def carries part keys at record +0x84 and the model
builder sub_4A5ED0 resolves each one here, so with no 0xC1 on the wire the
lookup returns null and every driver plus kart model build fails.
wire int32 | int32 | int32 lookup key | int32 | cstr model name
| int32 | int32 | int32 | cstr | cstr | 2x8 bytes | int32 count
0xC1 is the ITEM def container, sub_4510C0 resolves item keys against it.
The skin and part container is 0xC2, handler sub_47F800, stride 0xDC, and
sub_490A70 bails when a kart skin key misses it. The old name pointed at the
wrong one, keep both so nothing reads the wrong container by accident.
```

## tests/engine/test_item_types.cpp

Line 404

```
---------------------------------------------------------------------------
Test 17: Rubber-banding factors, flt_5EB718
---------------------------------------------------------------------------
```

## tools/clientpatch/hooks/QuestHook.h

Line 1

```
@file QuestHook.h
@brief restores the Quest and Room Craft buttons on the KnC top bar
CTopBar::Init (sub_42CF50 @0x0042CF50) is inline hooked with a hand rolled
six byte trampoline. The detour runs the original FIRST because the original
calls sub_44CC50 which zeroes all 50 button slots, then appends two records
through the stock registration helper sub_44C580. Every stage init re-enters
CTopBar::Init so the append has to run on every call, never once.
Target is KnC exe, x86 32 bit, image base 0x00400000. Every absolute address
is a named constant at the top of QuestHook.cpp carrying its evidence.
Nothing is written until the expected original bytes are verified byte for
byte, and a mismatch refuses the install instead of guessing.
Quest is command 15 and is fully client side: the dispatcher sub_42BCE0
routes it to sub_404410(app, 26) with no packet, no MSG_WAIT and no gate.
Room Craft is command 9 and is NOT self contained: it opens the MSG_WAIT
modal then sends an EMPTY C2S 0x010E. The emulator MUST answer with an empty
S2C 0x010E or the client hangs on that modal forever. Ship Quest alone if the
server side is not ready.
```

## tools/clientpatch/pilot.cpp

Line 1

```
@file pilot.cpp
@brief agent control channel, pipe thread plus game thread command drain
How the client reads input, proved from the decompiled exe
sub_404D60 is the only window proc body. Every message above 0x1C except
WM_TIMER falls through to LABEL_10, which calls the ime manager
sub_447800 and then the per stage handler chosen by *(0xB23288+900),
the same dword the pilot reports as "stage".
lobby handler sub_409500 switches on the raw message id
0x100 WM_KEYDOWN -> sub_409120(ctx, wParam) gated by 0xB23614
0x200 WM_MOUSEMOVE -> sub_408DA0(ctx, x, y)
0x201 WM_LBUTTONDOWN -> sub_409260(ctx, x, y)
0x202 WM_LBUTTONUP -> sub_409360(ctx, x, y)
0x203 WM_LBUTTONDBLCLK, 0x204 WM_RBUTTONDOWN, 0x20A WM_MOUSEWHEEL
so window messages are the event source and PostMessage or SendMessage
is the right mechanism. DINPUT8 is only used for the keyboard state array
sub_44B4C0 and the mouse buttons, not for ui events.
the x and y the handler uses do NOT come from lParam. They come from
sub_44B830 and sub_44B880 on the cursor object 0xE52688, which the frame
update sub_405120 refreshes once per frame through sub_44B770 ->
sub_44B6E0, and that one calls GetCursorPos. So to click a pixel we pin
the cursor object fields for the length of one synchronous SendMessage
and restore them, instead of moving the real cursor.
sub_44B830 divides the pinned value by the viewport scale at 0xD6E1E0 so
the ui hit tests in a 1024x768 space while our numbers are client pixels,
the same space the screenshot uses.
WM_KEYDOWN is dropped when 0xB23614 is zero, that byte is set from
WM_ACTIVATE and WM_ACTIVATEAPP in sub_404D60, so an unfocused window
eats keys. key and text raise it for the duration of the send and put
the old value back.
```

## tools/tests/intro/test_intro_render.cpp

Line 226

```
Use wrapper methods directly - they work!
wrapper[3] = sub_10016450 -> BeginPaint (inner[19])
wrapper[4] = sub_10016470 -> something
wrapper[5] = sub_100164A0 -> SwapBuffers? (inner[51])
wrapper[6] = sub_100164D0 -> GetWidth (inner[24])
wrapper[7] = sub_10016500 -> GetHeight (inner[25])
```


## 2026-09-15 correction, the unknown fields outside the catalogues chased to their readers

Method: the S2C handler stores the dword, every xref of the store or of the record accessor
was walked in KnC.exe.raw, a field with no read anywhere is written up as no reader in this
build. The results are on the opcode pages and in PACKET_REGISTRY 5.7, this block only lists
what earlier notes got wrong.

- 0x0045 third dword is a ping in ms, not a constant to copy. sub_4B4E60 hands it to
  sub_447AE0 (now ping_icon_draw) which picks Icon/link_0..4, 30 lands on the best bar.
- 0x002D value_1..7 are player_count, max_players, game_mode, has_password, channel,
  playing_flag, time_left_ms, read by sub_407B50 (lobby_room_row_draw). 0x0031 rewrites the
  first two, 0x0023 rewrites playing_flag, 0x0118 kind 1 rewrites has_password.
- 0x0013 field after game_mode is the has_password mirror 0xBCE224 that 0x0118 kind 0
  rewrites, it and the next three stores have no reader.
- 0x00CB is the owned item row verbatim from rec+0x00. sub_4516D0 returns the record base,
  not base minus 4, so the old SwapTicket struct read the instance id as a leaked dword and
  the base key as a template id.
- The owned rows carry the 0xC6 price row key at char +0x1C, kart +0x28, item +0x08,
  part +0x0C, pet +0x0C. sub_45E3D0 resolves it through sub_451E00, sub_415FF0 and
  sub_472490 draw the period and price triple from it. Kart row +0x08 +0x0C +0x10 are 0xC2
  part keys and +0x20 +0x24 are 0xC1 item keys, sub_490A70 resolves them, slots 3 to 5 have
  no reader.
- 0x0103 +0x0C is a pendant key, sub_460A10 checks it against the 0x011A owned pendant
  container 0x1A95244, the pet_condition table was a parallel invention.
- 0x011A +0x00 is the owned pendant instance id, sub_4511D0 erases by it, sending 0 on
  every row erases the wrong pendant on a reward.
- 0x00F3 +0x0C is the 0xC3 track id, stage 0x11 loads the world from it through 0xC70A4C.
  +0x1C is the entry fee, +0x20 +0x24 the mileage and exp numbers, +0x2C the reward key,
  +0x38 +0x59 +0x7A three ascii string keys, the third gets START SUCCESS or FAIL appended.
- 0x00F8 +0x04 does not pick MSG_QUEST_SUCCESS, sub_42E330 picks it from the stage result
  byte before the frame arrives, 0xC70A34 has no reader.
- 0x008D is sent by popup kind 0x1F only, object 0xEAB878, which has no open path in the
  image, so the shipped client never produces it. The MISSION_ENTER confirm is kind 0x2E and
  sends 0x0090.
- 0x0087 +0x1C and +0x20 carry the UNIT_MILEAGE and UNIT_EXP labels in code, the mission
  menu composes them and the scenario HUD draws them. The server pays +0x20 as exp now.
- The earlier 5.7 row for 0x0119 badge and unknown_04 was stale, the pendant record has
  no such dwords, every field of the packet has a reader.

## Correction 2026-09-15, catalogue fields settled on the readers

The unknown dwords of S2C 0xBF 0xC0 0xC1 0xC2 0xC3 0xC5 0xC6 were followed from their containers
into every reader, the results are on the opcode pages. Corrections to the notes above:

- 0xC0 offset 0x10 is the vehicle kind (car+0x33B4), offset 0x14 the model scheme, offset 0x1C the
  required player level against byte 0x1A20B09, offset 0x41 and 0x62 the def_trans title and
  description keys, offset 0x84 eight default skin keys, offsets 0x130 and 0x138 two ability pairs.
  Offsets 0x0C (one byte) and 0x18 have no reader in this build.
- 0xC1 offset 0x14 is the icon png stem, offset 0x35 the title key, offset 0x56 the description key.
  The server wrote them in the other order until 2026-09-15.
- 0xC2 offset 0x0C is the required level, 0x34 the restrict target, 0x38 the equip slot and shop tab,
  0x3C the restrict key. Migration 013 said 0x0C carried the category, wrong.
- 0xC3 the "camera_a b c" scalars are the three physics tuning floats, 0x44 difficulty stars, 0x48 the
  required license class against byte 0x1A20B08, 0x4C special mode only, 0x54 fog near, 0x58 fog far
  and the camera far clip, 0x5C 0x60 0x64 the lens flare sun position, 0x3C no reader.
- 0xC5 offsets 0x3C and 0x40 are the reward item type and key, 0x4C 0x50 fog near and far, 0x54 to
  0x5C lens flare, the three strings and 0x00 0x38 0x44 0x48 have no reader.
- 0xC6 offsets 0x0C 0x10 0x14 0x18 are unit type, unit amount, price base, price sale, 0x04 and 0x08
  have no reader.
- The stat bonus scale of `stat_bonus_add_part` uses the part grade, not a durability, and reads the
  0x0108 record, not 0xC2.
