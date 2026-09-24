# Item Drop Table Verified

What an item box gives and who decides it. Byte level ground truth from the
loaded binary plus the shipped exe data. Answers one question, can the server
change what a player wins.

## Source of truth

| Fact | Value |
|------|-------|
| Authoritative dump | `DevClient/KnC-new.exe.c` |
| Cross check | Ghidra `KnC.exe` image base 0x400000 |
| Roll | `sub_4B0570` @0x4B0570 |
| Weight table | VA 0x5EBE50 file offset 0x1EBE50 in .data |
| Standings table | `dword_2EBD6A0` rows at container +3520 bytes |
| Rank write | `sub_4B4B00` @0x4B4B00 from S2C 0x45 |
| Rank seed | `sub_4B5160` @0x4B5160 from S2C 0x0D |
| Rank read | `sub_4B4B50` @0x4B4B50 own rank, `sub_4B4B80` car at rank N |

## TL;DR

The box, the roll and the odds all live in the client. The server owns exactly
one input, the RANK, and rank picks the column of a table baked in the exe.

| Question | Answer |
|----------|--------|
| Can the server pick the item | NO, no grant packet exists |
| Can the server move a box | NO, boxes come from itembox.ini on disk |
| Can the server change the odds | ONLY through rank in S2C 0x45 and racer count |
| Where are the odds | baked at 0x5EBE50, patchable only in the exe |

## The standings table

`dword_2EBD6A0` holds 16 rows of 3 dwords starting at container byte 3520.

| Off | Field | Written by | Read by |
|-----|-------|-----------|---------|
| +0 | player id, the row key | `sub_4B5160` seed | `sub_4B4B00` search key |
| +4 | rank, zero based | S2C 0x45 field 2 | the roll and rank targeting |
| +8 | unknown, seeded zero | S2C 0x45 field 3 | no reader found |

S2C 0x45 carries three int32 `[playerId][rank][field3]` and `sub_4B4B00`
SEARCHES for the row whose key equals playerId then writes the other two. A
player id with no row makes the write a silent no op, so S2C 0x0D must seed the
board first. The shipped start chain 0x14 0x3E 0x0D 0x3A already does that.

Rank is zero based, PROVEN two ways. `sub_4B4B80` scans for rank 0 first when it
resolves the leader, and `sub_4B0570` treats rank 0 as its own branch with a
dedicated table.

Only three writers of the rank field exist in the whole image, the 0x0D seed,
the 0x45 write and the internal sort `sub_4B4BE0`. So ONLINE the S2C 0x45 is
the only thing that ever moves a rank. Stop sending it and every racer keeps its
grid index as rank for the whole race, which freezes the item odds on the
starting order.

## The roll

`sub_4B0570` returns an item id 0..21 or -1 for nothing.

```
racers = sub_48DED0(roster)             count of active cars
if theme is 30000000 battle             return 10 rocket or 18 bomb, no rank
if mode 1 or 3                          round racers up to even then cap 8
rank = sub_4B4B50(standings)            own rank zero based
if rank > 29                            return -1 no item
roll = rand() modulo 1000
walk the 22 item weights, accumulate, take the first item whose sum passes roll
if the walk runs off the end            return 17 hammer
```

Two special cases sit on top.

| Case | Rule |
|------|------|
| rank 0 draws 7 or 13 while already holding one | reroll excluding 7 and 13 |
| result is 2 spike | becomes 19 dung when the car field at `dword_1B1C43C` is 700 |

Slot capacity is 2, or 3 when the player owns template 5000 with quantity above
zero. That matches `ItemPackets::kThirdSlotTemplate`.

## Weight table layout

Base 0x5EBE50, int32 weights out of 1000, dword index:

```
index = 64*item + 1408*modeSel + 8*racerCount + rank
```

| Dimension | Range | Stride in dwords |
|-----------|-------|------------------|
| rank | 0..7 | 1 |
| racerCount | count of active cars | 8 |
| modeSel | 0..3 | 1408 |
| item | 0..21 | 64 |

VALIDATED, every row of 22 item weights sums to exactly 1000, and a racerCount
of N fills exactly N ranks and leaves the rest zero. A wrong formula could not
produce that.

## Only table zero is live

`modeSel = (byte_B23182 == 1) + 2 * (dword_2EB98B4 > 0)`.

| Global | Xrefs in the image | Verdict |
|--------|--------------------|---------|
| `dword_2EB98B4` | exactly ONE, a read inside the roll | never written, always 0 |
| `byte_B23182` | 20, every one a READ | no writer found, assume 0 |

The 0xB23180 writes are single byte writes to 0xB23180 itself, and S2C 0x14
`sub_479CC0` reads five int32 into B23170 B23174 B23178 B23198 B2319C which
never covers B23182. So tables 1 2 and 3 are unreachable in this build.

CONSEQUENCE, Angel 12, BlueRabbit 13, Devil 20 and DevilRed 21 carry weight only
in tables 1 2 and 3, so no item box ever gives them. BigBooster 1 is zero in
every table. Dung 19 is reachable only through the spike substitution above.

FALSIFIABLE, see an Angel come out of a box in game and this section is wrong.
A write through a computed pointer would not show as an xref.

## Live odds, table 0, weights out of 1000

Race with 4 cars.

| item | id | P1 | P2 | P3 | P4 |
|------|----|----|----|----|----|
| Booster | 0 | 40 | 100 | 120 | 250 |
| Spike | 2 | 240 | 10 | 10 | 0 |
| Storm | 3 | 140 | 10 | 10 | 10 |
| Thunder | 4 | 20 | 90 | 60 | 50 |
| Handle | 5 | 110 | 50 | 40 | 10 |
| Turtle | 6 | 10 | 100 | 100 | 120 |
| Rabbit | 7 | 10 | 50 | 80 | 150 |
| Shield | 8 | 120 | 50 | 50 | 10 |
| Smoke | 9 | 150 | 10 | 0 | 10 |
| Rocket | 10 | 30 | 120 | 160 | 130 |
| Hive | 11 | 10 | 50 | 20 | 0 |
| Ice | 14 | 50 | 100 | 80 | 30 |
| Flash | 15 | 20 | 10 | 20 | 10 |
| Magnet | 16 | 10 | 90 | 100 | 160 |
| Hammer | 17 | 20 | 120 | 100 | 30 |
| Bomb | 18 | 20 | 40 | 50 | 30 |

Race with 8 cars.

| item | id | P1 | P2 | P3 | P4 | P5 | P6 | P7 | P8 |
|------|----|----|----|----|----|----|----|----|----|
| Booster | 0 | 40 | 100 | 100 | 100 | 100 | 200 | 200 | 240 |
| Spike | 2 | 260 | 10 | 10 | 10 | 10 | 10 | 10 | 10 |
| Storm | 3 | 180 | 10 | 10 | 10 | 10 | 10 | 10 | 10 |
| Thunder | 4 | 10 | 110 | 60 | 110 | 50 | 100 | 60 | 100 |
| Handle | 5 | 100 | 50 | 50 | 50 | 30 | 50 | 10 | 10 |
| Turtle | 6 | 10 | 150 | 100 | 140 | 150 | 100 | 150 | 160 |
| Rabbit | 7 | 10 | 50 | 60 | 100 | 100 | 100 | 100 | 120 |
| Shield | 8 | 150 | 50 | 80 | 50 | 60 | 50 | 50 | 10 |
| Smoke | 9 | 120 | 10 | 10 | 0 | 10 | 10 | 0 | 0 |
| Rocket | 10 | 30 | 140 | 150 | 120 | 100 | 100 | 140 | 120 |
| Hive | 11 | 10 | 50 | 50 | 80 | 10 | 40 | 50 | 10 |
| Ice | 14 | 30 | 80 | 100 | 30 | 100 | 60 | 100 | 10 |
| Flash | 15 | 10 | 10 | 10 | 10 | 20 | 0 | 0 | 0 |
| Magnet | 16 | 10 | 100 | 80 | 100 | 100 | 100 | 100 | 180 |
| Hammer | 17 | 10 | 50 | 100 | 50 | 100 | 50 | 10 | 10 |
| Bomb | 18 | 20 | 30 | 30 | 40 | 50 | 20 | 10 | 10 |

Read the shape, it is rubber banding. The leader draws Spike and Storm and
Shield, the last place draws Booster and Magnet and Turtle and Rocket.

## What this means for the server

| Lever | Effect | Where |
|-------|--------|-------|
| S2C 0x45 rank | picks the column, the whole difference above | `RaceHandler::updatePositions` already sends it |
| racer count | picks the row, so a filler bot changes every human odds | `addBots` |
| S2C 0x0D | seeds the board, without it 0x45 writes nothing | start chain |
| the weights | need an exe patch, no packet reaches them | clientpatch |

The server cannot grant an item and must not try. It grants nothing and the
client tells it what it rolled with C2S 0x49.

## Out of band ranks and racer counts

The weight table gives each item 64 dwords, laid out as 8 racer counts of 8
ranks. Anything past that reads a neighbour row. Nothing crashes because the
whole table is 5632 dwords of live data, but the odds are silently wrong.

| Input | In band | Past it |
|-------|---------|---------|
| rank | 0..7 | reads the next racer count block of the same item |
| racerCount | 0..8 | 8*count walks into the next item, 16 lands two items away |

The client half protects itself, `sub_4B0570` clamps the count to 8 but ONLY in
modes 1 and 3, and it rejects a rank above 29 which is far looser than the table
really allows. So a race above eight cars gives wrong item odds in the modes
that do not clamp.

Room::maxPlayers defaults to 8 which is in band, but Room::kMaxGridSlots is 16
and a room that raises maxPlayers reaches ranks 8..15. A race row with
rank_in_race 16 and player_count 16 was observed in the test database, so this
path is reachable, not theoretical.

DO NOT clamp the rank in S2C 0x45 to hide this. The same field feeds rank
targeting, `sub_4B4B80` resolves the leader from it, so a clamped rank would
send thunder and hammer at the wrong car. Cap the race size instead.
