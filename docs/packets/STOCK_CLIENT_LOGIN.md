# Logging in with the stock client, UNSOLVED from the server side

Status 2026-08-22. NOT solved with an untouched `KnC.exe`. The exe is pristine,
byte identical to `KnC.exe.orig`, and the patch described further down was
REVERTED because the goal is no exe patch at all.

With the pristine exe and no DLL the login is a coin flip. Measured over
several batches, always the same bench:

| Configuration | Result |
|---------------|--------|
| pristine, defaults | 0/5 |
| pristine, revalidated after fixing the bench clicks | 1/3 |
| redirect 0x19 instead of 0x54 | 0/5 |
| login burst held 2000 ms after the 0xA7 | 1/5 |
| car part catalog off | 1/4 |

It is a race, not a deterministic failure, so a single trial proves nothing.
Always run a batch.

## The client bug

`FUN_00476A50` arms the socket read. It writes an in flight marker:

```
00476AAF  MOV dword ptr [ESI + 0x80A054], 1
```

That is the ONLY reference to `0x80A054` in the whole binary. Nothing reads it,
nothing clears it. The guard was started and never finished.

The consequence shows up on the login redirect. Opcode **0x54**, handler
`FUN_0047AA00`, reads int32 then an ascii host then int32 and calls
`FUN_004774C0` -> `FUN_00476FB0`, which closes the socket, **zeroes
BufferedCount at `0x80C058`**, opens a new socket and arms a read. Control then
returns to the parse loop `FUN_00476CC0`, which still subtracts the frame it
just handled. The 0x54 frame is 26 bytes, so the count lands at **-26**, and the
loop arms a SECOND read.

Two reads are now in flight on one OVERLAPPED, both writing at buffer plus zero.
The stream shifts and the client drops the connection. Measured: it died 9 to
11 ms into the login burst no matter the chunk size or the gap, 3500 or 900
bytes, 25 or 200 ms, always the first chunk.

## Server side levers tried, all measured, none fix it

- drip cadence, 25/3500, 60/1400, 120/900, 200/900. No effect at all, the
  client dies 9 to 11 ms into the burst on chunk 0 every time. So it was never
  volume or rate.
- redirect opcode. 0x54 `sub_47AA00` reconnects inside the parse loop, 0x19
  `sub_479340` only stores the target through `sub_405EB0` and lets
  `sub_405EF0` reconnect from the main loop. The deferred path measured WORSE,
  0/5. Kept behind `KNC_REDIRECT_0X19` for future work.
- holding the whole burst 2 s after the client sends 0xA7, `KNC_LOGIN_DELAY_MS`.
  1/5, no better than baseline.
- trimming catalogs with `KNC_CARPART_CATALOG=0 KNC_PART_UICAT=0`. The burst
  only drops from 39940 to 33329 bytes, still four times the client buffer.

## Why no send pattern can work, measured

One packet per write, `KNC_DRIP_CHUNK=64`, 297 writes instead of 12. Still
0 of 3. The spy shows exactly what happens, and it is not about what we send.

At the redirect the buffered count reaches -26, the size of the 0x54 frame, so
the client issues:

```
[ARM] count=-26 size=8218        ReadFile(buffer - 26, 8218) into a 8192 buffer
```

The receive buffer is `DAT_0080A058` and the OVERLAPPED it reads with sits
right in front of it at `DAT_0080A040`. Reading from `buffer - 26` writes 26
bytes of incoming data straight over its own OVERLAPPED, and reads 26 bytes
past the end as well.

After that the stream is shifted. Later:

```
[S2C] arrived=8192 total=8192
    head 00 00 00 00 00 00 00 00 F4 01 00 00 ...
    frame 0 op=0x0000 len=0        frame boundary lost, zeros parsed as frames
[ARM] count=13267 size=-5075       count past the buffer, read size negative, dead
```

The count is `0 minus the size of the redirect frame`. The handler zeroes it,
the parse loop then subtracts the frame it just handled. Nothing the server
sends changes that, and the smallest legal redirect frame still overflows.

So the historical "it used to work" was luck. The overflow happens on EVERY
login, and whether it kills the session depends on what lands on the OVERLAPPED
before it is reused. A small burst wins that race more often, which is why this
got worse as the login burst grew.

## The only remaining server side idea

Get the whole login burst under the client's 8192 byte buffer by loading the
catalogs lazily instead of dumping them at login. Not attempted. This also fits
the report that it used to work, the burst grew over time.

## The two halves of the fix, REVERTED, kept for reference

Half one, clamping the negative count, was already baked into `KnC.exe`:

```
00476A65  E8 D6 86 0B 00 90        CALL 0052F140 + NOP
0052F140  8B 8E 58 C0 80 00        MOV ECX,[ESI+0x80C058]
          85 C9  7D 08  33 C9      TEST, JGE +8, XOR ECX,ECX
          89 8E 58 C0 80 00  C3    store back, RET
```

Half two, the duplicate arm, only ever existed in the DLL. It is baked in now.

```
00476A50  E9 <rel32 to 0052F160> 90 90 90      was 51 56 8B F1 83 7E 08 FF

0052F160  83 B9 54 A0 80 00 00     CMP dword [ECX+0x80A054], 0
          74 06                    JZ  0052F16F
          B8 01 00 00 00           MOV EAX, 1        a read is already pending
          C3                       RET               so report success
0052F16F  51 56 8B F1              PUSH ECX, PUSH ESI, MOV ESI,ECX
          83 7E 08 FF              CMP dword [ESI+8], -1
          E9 <rel32 to 00476A58>   back into the original body

00476D00  E8 <rel32 to 0052F190> 90              was 8B 93 58 C0 80 00

0052F190  C7 83 54 A0 80 00 00 00 00 00   MOV dword [EBX+0x80A054], 0
          8B 93 58 C0 80 00               MOV EDX,[EBX+0x80C058]
          C3                              RET
```

`FUN_00476CC0` runs on completion with EBX holding the connection, so clearing
the marker there is what makes the guard cycle instead of latching.

Cave space starts at `0x0052F140` and runs 400 bytes of `CC` padding, plenty.

## Result of the exe patch

It worked, lobby reached with the player row intact. It is REVERTED anyway
because the requirement is an untouched exe. `DevClient/KnC.exe.prearmfix`
holds the patched build if it is ever wanted again.

## TRAP, do not restore the DLL as is

clientpatch hooks `kArmRecv = 0x00476A50`, which is exactly where the new JMP
lives. Installing it would trample the guard, and its trampoline would copy a
relative JMP to a new address where the displacement is wrong.

Before using the pilot again, either drop the arm hook from `spy.cpp` since the
exe does the job now, or restore `DevClient/KnC.exe.prearmfix` for that session.
