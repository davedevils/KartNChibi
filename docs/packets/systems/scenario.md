# Quest mode (scenario menu and quest race)

The stock client has a Quest mode: a menu of story quests, one race against a rival ghost per quest, a
reward on the first clear. The menu is stage 22 (ScenarioMenu, art `Quest_Back`, `UI_Quest_locked`), the race is
stage 17 (the Quest game). This page replaces the older scenario spec. Client `KnC.exe.raw`, image base
0x400000. Built on 2026-09-23, needs the new image and migration 069.

## 1. Flow

| # | dir | op | what | client |
|---|---|---|---|---|
| 1 | S2C | 0x00F3 x n | one quest definition each, after the 0x00BE wipe | `sub_47DAA0` stores 156 bytes, `FUN_00452DA0`, cap 50, lookup `FUN_00452E30` |
| 2 | S2C | 0x00F4 | the rows of the character, full replace | `sub_47DB00` clears (`FUN_00452BB0`) then appends 8 byte rows, `FUN_00452C40` keeps them sorted by key highest first, cap 50 |
| 3 | C2S | 0x011C | the Quest button | empty body, sender `sub_483970` |
| 4 | S2C | 0x011C | the menu ack | `sub_47E8E0` pushes stage 22 (`FUN_00404410(0x16)`), no op while `dword_F727F4` is 2 |
| 5 | C2S | 0x00F5 | Start on a row | `FUN_00437F30`: on any row but the last the row under it (the next lower key) must be cleared, then MSG_WAIT and the sender `sub_483520` with the key |
| 6 | S2C | 0x00F6 0x00F7 | the rival ghost replay | frame count then the chunks, stage 17 spawns the rival only when car 1 holds frames |
| 7 | S2C | 0x00F5 | start | `sub_47DD10` looks the key up in the 0x00F3 table and derefs the record with no null test, puts the gold of the wire in `DAT_0080E664`, opens stage 17 |
| 8 | C2S | 0x00F8 | the result, key then the race time in ms | sent from the race end of stage 17, the client then waits in state 2013 |
| 9 | S2C | 0x00F8 | the result | `sub_47DFA0`, below, sets state 2015 |
| 10 | S2C | 0x00F9 | a row opened by a level up | `sub_47E350` appends the row and arms the NEW badge |

## 2. 0x00F3 the quest definition, 156 bytes

| off | field | reader |
|---|---|---|
| 0x00 | 0 | no reader |
| 0x04 | quest key | `FUN_00452E30` every lookup |
| 0x08 | 0 | no reader |
| 0x0C | 0x00C3 track | the detail thumbnail and the stage 17 world, an unknown track pops `Track initialize fail` |
| 0x10 | 0x00BF rival driver | HUD portrait and the ghost car body, `sub_4B5EB0` derefs the driver row with no null test |
| 0x14 | 0x00C0 rival kart | the ghost car, stage 17 init fails on an unknown kart |
| 0x18 | 0 | lands in 0xC70874, no reader |
| 0x1C | entry fee | drawn after MSG_QUEST_PAY by `sub_438720` while the row is not cleared |
| 0x20 | gold | UNIT_MILEAGE on the reward line, `sub_4B5A00` |
| 0x24 | exp | UNIT_EXP |
| 0x28 | reward category | 0 driver, 1 kart, 2 item, 3 part, 4 pet, 5 room object, 6 car craft part, 7 pendant, 8 and up none; picks the icon (`sub_4438D0`) and the tail of 0x00F8 kind 3 |
| 0x2C | reward key | the catalogue key, `sub_438720` derefs its row with no null test so category 8 goes with no key |
| 0x30 0x34 | 0 | no reader |
| 0x38 | title key, 33 byte slot | `def_quest_index` key of the list row, `sub_4383D0` |
| 0x59 | info key, 33 byte slot | the detail line, `sub_438720` |
| 0x7A | story key, 33 byte slot | `sub_4B5EB0` copies it into 36 bytes and appends START, SUCCESS or FAIL, so 27 letters at most |
| 0x9B | pad | |

The rival name goes through `swprintf` into 14 wchar in `sub_47DD10`, so the rival driver name must be 13
letters or less. `Prince Waddles III` is out.

## 3. 0x00F8 the result

```
+0x00  i32  kind      0 failed, 1 cleared again, 2 paid, 3 paid with an item
+0x04  u8   flag      1 plays the success scene and MSG_QUEST_SUCCESS, anything else the fail
+0x05  u32  key
+0x09  u32  0         the high half of the key, no reader
 kind 2 and 3:
+0x0D  i32  gold after   into DAT_0080E664
+0x11  i32  exp after    into DAT_0080E65C
 kind 3 only, by the def category at +0x28:
 0 blob 0x2C driver, 1 blob 0x38 kart, 2 3 4 blob 0x1C (4 goes to the item container, a client slip),
 5 blob 0x30 room object, 6 blob 0x84 car craft part then u8 flag then blob 0x34 when the flag is 1,
 7 blob 8 pendant instance then key, drops the key (sub 451250) then appends (sub 451140)
size   13, 21, or 21 plus the tail
```

Kinds 2 and 3 set the row of the key cleared through `FUN_00452D30` with no null test, so the key must sit in
the 0x00F4 rows.

## 4. Our server

`ScenarioHandler` and `ScenarioPackets`, rows in `scenario_def` and `scenario_key_progress`, seeds in
migration 069.

- Login: the 0x00F3 defs ride every catalogue pass after 0x00BE, the 0x00F4 rows of the character follow
  (`ScenarioHandler::loginFrames`, a separate block of `GameServerLoginBurst.cpp`).
- `loadScenarioDefs` joins the rival driver, the kart, the track and the reward to the tables the login burst
  publishes and leaves out any def that would crash or stall the client (`defProblem`). Only the driver, kart and
  pendant rewards are granted, another category ships as none.
- The rows: every quest whose `required_level` is at or under the character level plus every cleared one, a new
  quest opens at every level (`visibleRows`), highest key first like `FUN_00452C40`.
- C2S 0x011C: 0x00F4 then the empty 0x011C (`handleMenuOpen`). A level up or a pendant earned by a first clear is
  pushed here once the menu is back.
- C2S 0x00F5: the def must exist and the row must pass the client gate (`rowPlayable`), the fee is charged while
  the row is not cleared (MSG_NO_MONEY otherwise), the rival ghost goes out on 0x00F6 0x00F7
  (`GhostHandler::sendQuestGhost`, the best replay of the quest track, `ghost_quest_replay`), then 0x00F5 with the
  gold after the fee. A refusal turns MSG_WAIT into an OK box.
- C2S 0x00F8: the run must match the started quest, the time must be 20 s or more, not past the server clock by
  more than 5 s, and beat the rival ghost time or the goal time when set (`decideResult`). The first clear flips
  `completed` in one guarded update so it pays once: gold, exp and the item, kind 3 with its tail or kind 2 when
  there is no item. A repeat win answers kind 1, a loss kind 0. Rows opened by the level up go out on 0x00F9.
- Pendants: quests 5 10 15 20 reward the pendants 02 03 04 05 (Rosie, Chai, Porki, Dim Dim) with category 7, the
  tail of kind 3 appends the row, `owned_pendant` is written first so the catch up of
  `ProgressionHandler::grantEarnedPendants` (`questPendantFor`) finds it owned and sends nothing twice.

Migration 069 seeds the 20 quests of `def_quest_index` (`Quest_NN_TITLE`, `Quest_NN_Info`, `QUEST_NN_STORY`), one
per level, rivals from the stories (Frankie, Madea, Jacko, Buttercup, Huck, Brag, Cleo), tracks of our catalogue,
fees 0 to 950, gold 300 to 2200, exp 200 to 2100, and one `ghost_quest_replay` row per quest on the same track.

Migration 069 was applied twice on a scratch MariaDB 11.1 loaded with a read only dump of the package tables, and
the def query of `loadScenarioDefs` then keeps all 20 quests: every rival driver, kart and track resolves and the
four pendant rewards find their `pendant_def` row.

Tests: `tests/server/test_quest_mode.cpp` (the record offsets, the result frames and tails, the lock rule, the
level rows, the result rule, the unsafe defs, the requests, the 069 seeds against `questPendantFor`).

The old handlers on 0x00C7 to 0x00CD (chapters, stars) stay as they were, no stock client sends 0x00C7 to 0x00CA.
