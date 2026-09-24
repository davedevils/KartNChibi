# Car craft preset rename and row update

Three packets of the car craft group: C2S `0x0114` rename request, S2C `0x0114` rename ack,
S2C `0x0124` row update. The rest of the group (C2S `0x010A` open, `0x010B` save, S2C `0x0107`
preset list, `0x0108` part definitions, `0x0109` part instances, `0x010A` open ack, `0x010B`
save result) is in `CustomCarPackets` and `CarCraftHandler` and is only context here.

Client reference: `KnC.exe.raw`, image base 0x400000.

## The factory model, 2026-09-24

Read off the exe, see [0x0107](../opcodes/0x0107.md) and [0x0109](../opcodes/0x0109.md) for the addresses:

- The top row of stage 18 is the 0x0107 list, one slot per row. A built row (slot_state 1) wears the
  chassis thumbnail and its name, an empty row is bare, the spots past the count show the forbidden
  sign. A click on a slot loads its config. The arrows scroll by one.
- One row per owned chassis: `sub_450140` finds the row of a kart and the model builder, the garage
  and the char panel read the parts from it. The garage lists only built rows of owned karts, then
  the karts that are no chassis.
- The chassis tab lists the owned karts of model scheme 1, the part tabs one row per owned instance
  of the category with its grade word, its period and the in use art. Install and Remove move the
  instance in the local config and the count at +0x0C, Save (`sub_42F3E0`) needs a chassis and a tire.
- The garage detail of a crafted kart lists the chassis and the installed parts in two columns and
  a mode 3 kart shows the wrench bar there and under the char panel (`sub_42AB70` at 67 516).

Our server holds that model: `syncFactoryLoadouts` gives every owned chassis a built row with a
BASIC set (the seven parts of its folder, permanent, grade 0) and period mode 3 at 500, the equip
count is the number of built rows holding a part, migration 072 applies it to stored rows. The
count `+1` `+2` the video shows on a part row has no draw in this build, our clone prints +0x0C.

## C2S 0x0114 rename request

Sender `FUN_00483790`, called from the text edit commit callback `FUN_00455840` (call site
0x4558DF) bound to the preset name field (`this+0x5f14` preset id, `this+0x5f18` the previous
name). When the text is empty or unchanged the commit does nothing. Otherwise the callback
converts the text to ANSI with a cap of 10 characters plus the NUL and, on a live connection
(`DAT_012124b8 != -1`), sends:

```
+0x00  i32   preset_id
+0x04  cstr  name, ASCII, NUL terminated, no length prefix
```

Offline (`DAT_012124b8 == -1`) it patches its cached record locally and sends nothing. The
client never waits for the answer.

## S2C 0x0114 rename ack

Handler `FUN_0047E620`:

```
+0x00  u32   preset_id
+0x04  cstr  name, copied into a 12 byte stack buffer with no bound
```

It looks the preset up with `FUN_004501D0` (stride 0x34, key at +0x00, name at +0x08) and
copies the name in without a null check on the lookup. An ack for a preset id the client never
received on `0x0107` crashes it, a name of 12 bytes or more smashes the stack.

## S2C 0x0124 row update

Handler `FUN_0047EB10` reads 0x34 bytes, looks the preset up by its first dword and, when it
exists, overwrites the whole cached row. A miss is ignored.

| Offset | Field |
|--------|-------|
| 0x00 | preset id, the lookup key |
| 0x04 | slot state |
| 0x08 | name, 12 bytes ASCII |
| 0x14 | kart instance id |
| 0x18 | cover part instance |
| 0x1C | tire part instance |
| 0x20 | booster part instance |
| 0x24 | bumper part instance |
| 0x28 | front fender part instance |
| 0x2C | rear fender part instance |
| 0x30 | wing part instance |

The same 0x34 byte record as one `0x0107` row, `CustomCarPackets::presetRecord`.

## Our server

`GameServerDispatch.cpp` routes C2S `0x0114` (`C_ENTITY_LIST_276`) to
`CustomCarPackets::parseRenameRequest` then `CarCraftHandler::handleRename`:

1. No character or a bad body is logged and dropped.
2. The preset must be one of the character's presets (`loadPresets`), else
   `MSG_UNSUPPORT` and no ack, so the client never receives an ack for an id it lacks.
3. `CustomCarPackets::renamePreset` checks the owner again, cuts the name to
   `PRESET_NAME_MAX` (9 characters, the width of the plate cell) and updates
   `custom_car_preset.name`.
4. S2C `0x0114` with the stored name, then S2C `0x0124` with the whole row so the slot label
   follows. A failed write still sends both with the old name, then `MSG_UNSUPPORT`.

`tests/server/test_client_reader_rules.cpp` runs both frames through the client reader rules
and checks the 9 character cut.

## Open questions

1. The stock server may have sent `0x0124` on other events too, for example when a part runs
   out under a saved preset. No capture shows it and our server sends it only after a rename.
2. The client caps the ANSI conversion at 10 bytes, a name with non ASCII characters was not
   traced. The server keeps ASCII only.
