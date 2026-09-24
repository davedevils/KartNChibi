# Equip vs Mission 0x8C Conflict Resolved

Byte level reverse to settle the 0x8C opcode fight. Prior garage doc claimed
0x8C = vehicle equip via sub_43AB00. Mission doc claimed 0x8C = mission
completion via sub_43AB00. Same function same send site. One of them is wrong.
This doc proves which.

## VERDICT

VERDICT B. 0x8C is MISSION ONLY both directions. Equip is a DIFFERENT opcode.

- C2S 0x8C 140 has ONE sender sub_43AB00 at run state 3000 mission finish report
- S2C 0x8C 140 sub_47B9E0 is mission complete ack mark done plus reward
- Real garage EQUIP is opcode 0xB9 185 action 0 vehicle sender sub_4842F0
- Server C_EQUIP_VEHICLE=0x8C is a pre existing bug it steals the mission opcode

## Source of truth

| Fact | Value |
|------|-------|
| Authoritative dump | DevClient/KnC-new.exe.c full Hex-Rays |
| Live check | Ghidra MCP program KnC.exe image base 0x400000 |
| C2S builder | sub_44ECD0(pkt, opcode) writes size u16 opcode u16 then payload |
| Field writer | sub_44E9C0(src, N) append N raw bytes |
| S2C reader | sub_44E910(dst, N) read N bytes |
| Send now | sub_476B80(net, pkt) direct |
| Send confirm popup | sub_405FE0(pkt, ms) |
| Cross checked | PACKET_REGISTRY.md |

---

# 1 the 0x8C C2S sender is sub_43AB00 mission finish PROVEN

sub_43AB00 @0x43AB00 is NOT a kart select screen. It is the mission GAME update
state machine. State field is param_1[0x246]. States seen 0x3f2 0x44c 1000 3000
0xbc2 0xbd6 0xbe0 0xbea 0xfaa 0xfb4 4000. It walks track waypoint names via
sub_4A0680 comparing to "START" and "FINISH" strings then compares a target
sub_450CA0(*param_1)+0x10 to a run counter param_1[2]. That is mission gameplay.

Only send in the whole function is at state 3000.

```
iVar5 = sub_450BA0(*param_1)        mission PROGRESS row by missionId key
param_1[5] = *(iVar5 + 4)           local read of done state
sub_44ECD0(0x8c)                    opcode 140
sub_44E9C0(param_1, 4)              append 4 bytes = *param_1 = missionId
sub_476B80(pkt)                     flush
```

| C2S 0x8C payload | bytes | meaning |
|------------------|-------|---------|
| missionId | i32 | *param_1 current mission id PROVEN |

sub_450BA0 is the mission progress accessor row by missionId key see
PACKET_REGISTRY.md. So the 4 byte field is a missionId not a kart
uniqueId. The garage doc label kart uniqueId was a misread of the same send.

PROVEN there is exactly ONE 0x8C C2S send site and it is mission completion.

---

# 2 the 0x8C S2C handler is sub_47B9E0 mission ack PROVEN

Dispatcher stub at 0x477dc6 does CALL 0x47b9e0 bytes e8153c0000. Xref shows
sub_47B9E0 has ONE caller that stub. Dispatcher is the opcode jump table at
0x4777C0 case 140. So S2C 0x8C 140 maps to sub_47B9E0 @0x47B9E0.

sub_47B9E0 read order.

```
sub_44E910(&result, 4)              result flag 0 or 1
sub_44E910(&idblk, 8)               idblk[0] = missionId then 4 pad
if result == 0 or result == 1:
    row = sub_450BA0(idblk[0])      mission PROGRESS row by missionId
    row+4 = 1                       MARK MISSION DONE
    sub_44E910(&gold, 4)
    sub_44E910(&cash, 4)
    DAT_0080E664 + base = gold
    DAT_0080E65C + base = cash
if result == 1:
    def = sub_450CA0(idblk[0])      mission DEFINITION row by missionId
    switch def+0x28 reward icon type 0..7:  read typed reward insert inventory
sub_43A1B0(0xbc2)                    mission game to RESULT screen
```

Key proof this is mission not equip.

| Evidence | Why it is mission | Ref |
|----------|-------------------|-----|
| row lookup sub_450BA0(idblk[0]) | mission progress table keyed by missionId | sub_450BA0 |
| row+4 = 1 | sets mission progress done flag | progress row +4 = state |
| def lookup sub_450CA0(idblk[0]) | mission definition table keyed by missionId | sub_450CA0 |
| switch def+0x28 | mission definition reward icon type 0..7 | def +0x28 |
| final sub_43A1B0(0xbc2) | forces mission game to result screen state | 0xbc2 in sub_43AB00 |

The item inserts in cases 0..7 are REWARD grants into inventory stores not equip
results. They are keyed by the mission definition reward type not by an equipped
slot. And the tail sub_43A1B0(0xbc2) drives the mission RESULT screen. A garage
equip reply would never force a mission result screen. PROVEN mission only.

Note the garage doc read the head field as 0 gold only 1 full add and called
the id an equipped item id. Both are the same wire bytes but the id is the
missionId and the switch is the mission reward type. Same layout different name.

---

# 3 the REAL equip opcode is 0xB9 185 action 0 PROVEN

Garage screen INSTALL button sends 0xB9 185. The action int selects category.
action 0 = vehicle kart so equipping a kart for the race is 0xB9 action 0.

## Install button to sender wiring PROVEN

sub_412BC0 @0x412BC0 is the INSTALL button dispatcher region 2. It reads the tab
state then calls the sender sub_4842F0.

```
key = sub_412AB0(row)               resolve clicked row id
main = *(this + 0x16330)            main category tab
sub  = *(this + 0x16334)            sub tab
main==0 sub==0 : sub_4842F0(0, key, -1)    action 0 VEHICLE
main==0 sub==5 : sub_4842F0(4, key, -1)    action 4 pet
main==1        : sub_4842F0(1, key, ...)   action 1 item
main==2        : sub_4842F0(2, key, -1)    action 2 durability repair kit
else           : sub_4842F0(3, key, -1)    action 3 accessory
```

Vehicle tab is main 0 sub 0. That path sends action 0. So EQUIP VEHICLE is
0xB9 action 0. PROVEN.

## Install sender body PROVEN

sub_4842F0 @0x4842F0 builds the packet.

```
sub_4641E0("MSG_WAIT", 0)           block ui
sub_44ECD0(0xb9)                    opcode 185
sub_44E9C0(arg action, 4)
sub_44E9C0(arg key, 4)
sub_44E9C0(arg extra, 4)
sub_405FE0(pkt, 600)                send after confirm popup
```

| C2S 0xB9 payload | bytes | meaning |
|------------------|-------|---------|
| action | i32 | category 0 vehicle 1 item 2 durability 3 accessory 4 pet |
| key | i32 | selected inventory row id |
| extra | i32 | -1 normal else current equipped kart uniqueId on slot swap |

So equip a kart = opcode 0xB9 with action 0 key = kart row id extra -1. PROVEN.

---

# 4 side by side

| Question | Answer | Proof |
|----------|--------|-------|
| Does equip send 0x8C | NO | 0x8C only sender is sub_43AB00 mission finish |
| Does mission finish send 0x8C | YES | sub_43AB00 state 3000 sub_44ECD0(0x8c) |
| Is 0x8C overloaded C2S | NO one sender one meaning | single send site |
| Is 0x8C overloaded S2C | NO sub_47B9E0 mission ack only | progress plus def plus result screen |
| Real equip opcode | 0xB9 185 action 0 | sub_412BC0 to sub_4842F0 |
| Real equip payload | i32 action i32 key i32 extra | sub_4842F0 three sub_44E9C0 |

---

# 5 server fix

0x8C must route to mission completion NOT equip. Equip must route to 0xB9.

| Server const now | Reality | Correct |
|------------------|---------|---------|
| C_EQUIP_VEHICLE = 0x8C | 0x8C is mission finish report | remap equip to 0xB9 185 |
| mission complete | already 0x8C but shadowed by equip bind | give 0x8C to mission handler |

Server routing rule for opcode 0x8C 140.

```
C2S 0x8C payload = { i32 missionId }
   look up mission progress by missionId
   validate the run pass see PACKET_REGISTRY.md
   reply S2C 0x8C { i32 result, i32 missionId, i32 pad, i32 gold, i32 cash, [reward struct] }
```

Server routing rule for equip.

```
C2S 0xB9 185 payload = { i32 action, i32 key, i32 extra }
   action 0 vehicle 1 item 2 durability 3 accessory 4 pet
   action 0 = equip kart key = kart uniqueId extra = -1 or swapped slot id
```

No disambiguation of 0x8C is needed because equip never uses 0x8C. The two only
collided because the server bound C_EQUIP_VEHICLE to the mission opcode.

---

# 6 what the equip S2C reply is INFERRED

sub_47B9E0 is NOT the equip reply it is mission. The equip result path was not
pinned in this pass. Equip moves an item between inventory and an equipped slot
so the refresh most likely arrives as an inventory resend not a dedicated equip
ack. Candidate S2C 0x1B 27 handler sub_478EC0 vehicle inventory resend or the
add remove pair 0x9D 157 sub_47C2D0 add vehicle and 0x9E 158 sub_47C320 add
item. INFERRED not proven here. Follow up needed to name the exact 0xB9 ack.

---

# PROVEN vs INFERRED

PROVEN
- sub_43AB00 is the mission game state machine START FINISH waypoints states
- 0x8C C2S has one send site sub_43AB00 state 3000 payload i32 missionId
- 0x8C S2C sub_47B9E0 marks mission progress row+4=1 grants reward ends at result
- dispatcher case 140 calls sub_47B9E0 bytes at 0x477dc6 e8153c0000
- equip vehicle is 0xB9 185 action 0 via sub_412BC0 to sub_4842F0
- 0xB9 payload is i32 action i32 key i32 extra
- 0x8C is NOT overloaded either direction

INFERRED
- exact S2C reply for the 0xB9 equip likely inventory resend 0x1B or add pair
- server should recompute the mission pass client only sends missionId
