# What chibikart.gg patched in their released client

Their `KnC.exe` is byte for byte the same build as ours, 7421952 bytes, with
**1068 bytes changed across 64 runs**. Diffed against `DevClient/KnC.exe.orig`.

## Everything they changed

| VA | What |
|----|------|
| `.rdata 0x5A1564`, `0x5A1590` | the forgot password and register URLs, now chibikart.gg |
| `0x52F210`..`0x52F260` | new strings in the code cave, site and discord URLs, `Menu/Common_` art keys |
| `0x42BEB4` `0x42BF70` `0x42BF80` `0x42C523` `0x42C5D6` `0x42C5E3` `0x42C5ED` | `PUSH <url>` then `CALL` in place of the old handlers, the Shop and Gacha buttons open the web site now |
| `0x5300A0`..`0x5301FC` | a new import block for `KncPresence.dll`, discord rich presence |
| `0x4993E2`..`0x4994B4` | nameplate draw, alpha `A8` to `E0` and offsets `0x1A`->`0x1C`, `0x0F`->`0x11` |
| `0x43E080`..`0x43E099` | different constants fed to the rng at `[0x59F38C]` |
| `0x43E115` -> cave `0x52F280` | **the only real code hook**, see below |
| `0x42CC8D` | 43 bytes NOPed, the same plate our `patches.ini` turns off at `0x42CC8B` |
| `0x406C9F` `0x411D6A` `0x411E81` `0x411F03` `0x495BC2` | small constant and branch edits |

## Their one code hook, a windowed resolution fix

```asm
0052F280  lea  edx, [esi + 0x38]
          push edx
          push dword ptr [esi + 8]      ; hwnd
          call dword ptr [0x59F36C]     ; GetClientRect
          mov  ecx, [esi + 0x40]        ; rect right
          mov  edx, [esi + 0x44]        ; rect bottom
          test ecx, ecx
          je   done
          test edx, edx
          je   done
          mov  [esp + 0x48], ecx        ; override render width
          mov  [esp + 0x4c], edx        ; override render height
          mov  ebp, ecx
done:     mov  ecx, 0x5E02B8
          jmp  0x43E11A                 ; back into the original
```

They also ship `Option2.ini` with `window_mode = 0.00` and `wide_mode = 0.00`,
ours has both at `1.00`.

## What this settles

**They patched NOTHING about character or kart rendering, and NOTHING about the
recv arm.** Their exe is stock plus branding, plus web buttons, plus discord
presence, plus that resolution hook.

So the stock client draws the chibi and the kart on its own when the server
feeds it correctly. The lobby stand fault is OURS, on the server side, not a
client defect that needs patching out.

## A correction this forced

They ship no `Data/` tree at all, only `pak001.dat` `pak002.dat` `pak003.dat`.
We have the identical `pak001.dat`, 909373514 bytes dated 2014.

That invalidates the reading in CLIENT_DATA_SEEDING.md. The spy hooks
`CreateFile`, and a read served from inside the pak never opens a file, so the
"3 successful opens against 2246 misses" was never evidence of missing assets.
The client was reading the pak the whole time and the misses are just the loose
file probe it does first.

The `Data/Eng` junctions stay, they are how loose overrides win over the pak,
which is what the hand seeded tree is for. They are simply not the fix that
earlier note claimed.
