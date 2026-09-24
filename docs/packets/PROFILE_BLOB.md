# The profile blob, read off the wire

S2C `0x0007` carries `[int32 charId][1224 byte blob]`, 1228 bytes of payload. S2C
`0x000A` writes into the same blob, field by field, from `+0x4A0` on. The blob
itself lives at `obj+0x80E1B8`, the u32 in front of it at `obj+0x80E1A8`, and it
survives the hop from the login server to the game server, so whichever packet
speaks last wins.

The head of the blob is a return ticket. `sub_480500` builds every C2S `0x00A7`
after the redirect out of the u32 at `+0x000` and the wstring at `+0x004`, so
the login server writes the account id and the session token there and the game
server matches its `active_sessions` row on both. Before 2026-09-11 the game
server matched on the client IP, which two players behind one router share.

Offsets below are blob relative. Payload offset is blob offset plus four.

## What the fields are

Settled by diffing our blob against a chibikart capture, `reference_login_burst.bin`,
733 frames from a server whose stock clients work.

| blob | field | proof |
|------|-------|-------|
| `+0x4A0` | channel level band, byte | theirs 2 |
| `+0x4A1` | level, byte | theirs 0 |
| `+0x4A4` | exp current | theirs 200, ours shows as the numerator |
| `+0x4A8` | astro | |
| `+0x4AC` | gold | ours 49910, the lobby read 49928 after spending |
| `+0x4B0` | **selected character, owned instance id** | theirs 10034, the id their `0x1B` carries |
| `+0x4B4` | **selected kart, owned instance id** | theirs 10035, the id their `0x1C` carries |
| `+0x4C0` | exp required for the next level | ours 500, the lobby read `/ 500` |
| `+0x4C4` | the worn pendant key, 0x01A20B2C, drawn over the char panel button and written by the `0x0123` ack | theirs 0 in the reference capture, the 1000 of the old row was the exp next at `+0x4C0` |

The two selection fields are instance ids, not base keys. `0x1B` and `0x1C` each
open with a count then the instance id then the base key, so the instance is the
second int32 of those containers, not the third.

## The 0x000A payload

Thirty eight bytes, packed, no padding, and the size is the proof the mapping is
right:

```
byte  hasPlayer -> +0x4A0      int32 gold      -> +0x4AC
byte  isGM      -> +0x4A1      int32 character -> +0x4B0
int32 exp       -> +0x4A4      int32 kart      -> +0x4B4
int32 astro     -> +0x4A8      4 bytes         -> +0x4B8..+0x4BB
                               int32 x3        -> +0x4BC +0x4C0 +0x4C4
```

Two bytes plus five int32 plus four bytes plus three int32 is thirty eight.

## What this cost us

Two separate bugs, both in the selection pair, and together they emptied the lobby.

The login server wrote the kart at `+0x4B0` and the character at `+0x4B4`, the
wrong way round.

Then `0x000A` overwrote both, and its kart came from
`characters.selected_kart_instance_id`, a column only the garage and character
creation ever write. A seeded account carried zero there so the kart went out as
minus one. The character next to it resolved through `equipped_driver_id` into
`owned_character` and was always fine.

The stand needs both halves before it draws either, so it drew neither, greyed
every button, and never raised an error. No server log and no screenshot could
point at it. The client was healthy the whole time, it just had nothing to place.

## The other thing the capture says

They never send `0x000A` at all. Everything the lobby needs is in `0x0007`. Ours
sends both, which is survivable now that they agree, but the shorter path is the
one their server takes.
