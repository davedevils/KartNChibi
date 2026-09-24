# The login burst, order not size

Extracted from a full chibikart capture, `hl_chibi.log`, 742 frames. This is a
server whose stock clients work, so its shape is the reference.

## What theirs does

Four phases, and the boundary between the third and the fourth is the client
answering.

```
1  catalogues, first pass
   0xBE   0xC6 x23   0xBF x10   0xC0 x46   0xC2 x335

2  catalogues, second pass
   0xC2 x138   0x103 x4   0xC1 x2   0x108 x35   0x10C x71   0xC4 x12   0xC3 x40

3  the player, only now
   0xF1 clock   0x07 session   0x1B own character   0x1C own kart   0x1D
   0x104   0x10D x2   0x0E

   -> the client replies TX 0x18

4  only after that reply
   0x76   0x78   0x79   0x95   0x96   0x02   0x11
```

Every catalogue is sent. Pets, items, carcraft, roomcraft, licences, all of it.
Nothing is lazy and nothing is trimmed.

## The same burst on 2026-09-11, uncut, through their launcher

`capture_2026-09-11.log`, 900 frames, no hex dump cap. Two additions
against the September 2 capture, the pendant definitions in phase two and the worn
pendant in phase three, and the lobby tail after the client's `0x12`.

```
2  0x103 x4   0x119 x13   0xC1 x2   0x108 x35   0x10C x71   0xC4 x12   0xC3 x40
3  0xF1   0x07   0x1B   0x1C   0x1D   0x104   0x123   0x10D x2   0x0E
4  0x76   0x78   0x79   0x95   0x96   0x02
   -> the client sends 0x12
   0x12   0x3C   0x02   0xAB   0xAC x40   0x11D
```

`0x119` is one pendant definition per frame, thirteen of them, the last hidden.
`0x123` is the worn pendant key, 0 for none. The `0x3C` after the lobby opens is
seventeen bytes, not the race reward form. Their login itself is `0xFA` then `0x07`
with the cstr version `2`, action 4, name and password, no launcher token on the
game socket.

## What matters, and it is not the volume

Their burst is 65 KB over the first six seconds against a client that reads into an
8192 byte buffer, eight times its size, and it works. So volume was never the story,
and an afternoon spent shrinking ours was an afternoon spent on the wrong thing.

Three rules come out of their order.

**The player's own data comes after every catalogue.** `0x07`, `0x1B` and `0x1C`
describe the character, and the client places them into structures the catalogues
have to have filled first.

**Nothing that restarts the client's state machine appears mid burst.** Ours sent
`0x07` and `0x04`, the session info and the character creation result, in among the
catalogue rows. The second one especially has no business there at all.

**Inventory and mail wait for the client to speak.** `0x76`, `0x78`, `0x79`, `0x95`
and `0x96` only go out after the client sends `0x18`. Sending them earlier means
writing into lists the client has not opened yet.

## What ours does

Twenty odd packet kinds from a dozen call sites, interleaved, with `0x07` and `0x04`
in the middle and the inventory going out unprompted.

## The dead end, kept so it is not tried twice

`KNC_MINIMAL_BURST=1` cuts the burst to the five kinds phase one carries. It
reproduces their first phase faithfully and it is wrong, because phases two to four
carry the player. With it on the lobby opens with no character, every mode button
greyed and EXP reading `49970 / 0`. Left in the tree switched off.

That symptom was read as a missing `0x0A` at the time and that was wrong. The
reference capture never sends `0x0A` once, so its client fills the exp bar from
`0x0007` alone. The empty lobby had nothing to do with the burst either, see
[`PROFILE_BLOB.md`](PROFILE_BLOB.md).
