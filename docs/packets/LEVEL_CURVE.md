# Level curve

The exp a character needs for each level, where every number comes from, and how the server and the
stock client use it. Pass of 2026-09-23, migration 071.

## Who owns the numbers

The client has no level table. The char info draw `FUN_00429990` prints `EXP : %d / %d` with two
profile fields and fills the bar from three:

| field | profile address | wire | meaning |
|---|---|---|---|
| exp | 0x01A20B0C | 0x000A +0x02, 0x0007 blob | cumulative exp of the character |
| floor | 0x01A20B24 | 0x000A +0x1A, 0x0007 blob | cumulative exp where the current level starts |
| next | 0x01A20B28 | 0x000A +0x1E, 0x0007 blob | cumulative exp where the next level starts, the `y` of `EXP : x / y` |

The bar is `(exp - floor) / (next - floor) * 256` pixels from 108 632 (`0x00429A02` to `0x00429A23`),
pinned full at 256 and once the wire level byte plus one reaches 50 (`0x00429A34`). The only xrefs to
0x01A20B24 and 0x01A20B28 are the two reads of that draw, the writes come from the 0x0007 and 0x000A
readers. No string of the exe names a level or exp table (`(level|exp).*\.(txt|ini|csv|dat|bin|tbl)`
finds nothing) and no file of `Data` or `Define` is named for one. So the server sends `y` and the curve
is ours to pick.

The client loads 50 level icons, `Icon/lv_icon_001` to `050` and their `_s` pairs, plus the GM crown
(`FUN_004425F0`). The pak ships `Lv_icon_001` to `055`. The Ongame archive has `Lv_icon_s_055.png` and the
chibikart.gg post names level 55 as the cap (docs/reverse/ARCHIVE_SOURCES.md, "Max level: 55"). Levels 51
to 55 draw the level 50 icon and a full bar in this exe, `ProgressionPackets::WIRE_LEVEL_MAX_SEND` (54) is
the wire byte of level 55.

## The sources

| source | what it gives |
|---|---|
| GOA forum tutorial, `kartncrazy.taguilde.net` t9 (docs/reverse/ARCHIVE_SOURCES.md) | the total exp of levels 1 to 17, EU build |
| the official server video `www.youtube.com/watch?v=ViGZzEWl5sE` | a level 45 GM showing `EXP : 1000000 / 1035000`, so level 46 starts at 1035000 |
| the same video | a low level player showing `EXP : 164 / 221` |
| the archive and the pak | the cap, level 55 |

The GOA post gives a total column and a "to next" column. They disagree once: level 15 says +3000 but
the totals give 17350 - 14500 = 2850. The totals are used.

The `164 / 221` point fits no level of the GOA table (level 2 starts at 100, level 3 at 300). The video
does not show the level of that player, so the point is not used. It hints that the build of the video
had its own low levels, nothing in the client or the archive can pin them.

## The fit

- Levels 1 to 17: the GOA totals as posted.
- Levels 18 to 55: the step to the next level grows by one ratio `r` per level, starting from the last
  GOA step (3450, level 16 to 17): `step(L) = 3450 * r^(L - 16)` for L = 17 to 54, `total(L+1) = total(L) + step(L)`.
- `r` is solved so that `total(46) = 1035000`: the 29 steps from level 17 to level 46 must add up to
  1035000 - 20800 = 1014200, bisection gives `r = 1.13031`.
- Every total is rounded to the nearest 50, like the GOA numbers, and level 46 is pinned at 1035000.
- A 13 percent growth per level is in line with the tail of the GOA table (2500 to 2850 is 14 percent,
  2850 to 3450 is 21 percent).

| level | total | level | total | level | total | level | total | level | total |
|---|---|---|---|---|---|---|---|---|---|
| 1 | 0 | 12 | 7900 | 23 | 53300 | 34 | 230950 | 45 | 914650 |
| 2 | 100 | 13 | 9800 | 24 | 61400 | 35 | 262250 | 46 | 1035000 |
| 3 | 300 | 14 | 12000 | 25 | 70600 | 36 | 297600 | 47 | 1171050 |
| 4 | 600 | 15 | 14500 | 26 | 81000 | 37 | 337600 | 48 | 1324850 |
| 5 | 1000 | 16 | 17350 | 27 | 92750 | 38 | 382800 | 49 | 1498650 |
| 6 | 1500 | 17 | 20800 | 28 | 106000 | 39 | 433850 | 50 | 1695150 |
| 7 | 2150 | 18 | 24700 | 29 | 121000 | 40 | 491550 | 51 | 1917250 |
| 8 | 2950 | 19 | 29100 | 30 | 137950 | 41 | 556800 | 52 | 2168250 |
| 9 | 3900 | 20 | 34100 | 31 | 157150 | 42 | 630550 | 53 | 2452000 |
| 10 | 5000 | 21 | 39700 | 32 | 178800 | 43 | 713900 | 54 | 2772700 |
| 11 | 6300 | 22 | 46100 | 33 | 203300 | 44 | 808150 | 55 | 3135200 |

Levels 1 to 17 are GOA, 18 to 55 are the fit. The GM of the video with 1000000 exp is level 45
(914650 up to 1035000) and our 0x000A carries floor 914650 and next 1035000, so the client prints
`EXP : 1000000 / 1035000` like the video.

The seed of migration 009 (level 2 at 500, 10 at 46500, 20 at 323000, 50 at 4532500) had no source,
its own header called the numbers synthetic.

## The server

- `level_curve` holds the 55 rows, migration 071 writes them and recomputes `characters.level`,
  `exp_floor` and `exp_next` (the new ladder is under the old one at every level, so a level only goes up).
- `util/LevelCurve.h` `levelOnCurve` and `boundsOnCurve` are the only math. `ProgressionPackets::levelForExp`
  and `expBoundsForLevel` call them, so the level stored after an award and the floor and next of the
  0x000A that follows come from the same rows.
- The top row has no successor, its next is floor plus one so the client never divides by zero.

Tests: `LevelCurve071.GoaHeadAndTheVideoPoint` (the 55 rows, the GOA head, level 46 at 1035000),
`LevelCurve071.TheLevelUpAndTheDisplayAgree` (the GM point, and for every level the first and the last
exp of the level give that level and a bar from 0 to 255 pixels). Needs the new image and migration 071.

Docker proof 2026-09-23 (scenario `videocheck`, `dock1` with 1000000 exp): the package image
draws level 30 and `EXP : 1000000 / 1029500`, the server of this change with 071 draws the level 45 badge
and `EXP : 1000000 / 1035000`, the numbers of the video.

Migration 071 moves every character to the level its exp reaches on the new ladder and never down: on
the package data `admin` goes from level 2 to 3 (537 exp, level 3 starts at 300).
