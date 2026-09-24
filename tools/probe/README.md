# knc_probe: live in-process packet probe

Injected 32-bit DLL that hooks the KnC client's (`DevClient\KnC.exe`) message path
with MinHook and dumps every parsed/sent packet to JSONL, at native speed, without
freezing the client (unlike the dbgeng/Ghidra live-trace path).

Ported from the X-Legend `hbo_probe`. The engine is game-agnostic; only `probe.cfg`
is KnC-specific. KnC.exe is x86 with PE image base `0x00400000` (no ASLR), so it
loads at `0x400000` and addresses from `docs/packets/PACKET_REGISTRY.md` map directly.

## Build (x86 only)

```
"<VS>\VC\Auxiliary\Build\vcvarsall.bat" x86
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -S tools/probe -B tools/probe/build
cmake --build tools/probe/build
ctest --test-dir tools/probe/build --output-on-failure
```

Outputs `knc_probe.dll` and `knc_probe_inject.exe` under `build/`.

## Use

1. Put `probe.cfg` next to `knc_probe.dll`.
2. Start `DevClient\KnC.exe`, then inject:
   ```
   build/knc_probe_inject.exe KnC.exe <abs-path>/knc_probe.dll
   ```
3. Read captures: tail the file set by `jsonl=` in probe.cfg (default
   `knc_capture.jsonl`), or connect to the live tap: `ncat 127.0.0.1 8077`.
   The tap is best-effort and may drop events under load; the JSONL file is the
   authoritative complete log. Note: a *relative* `jsonl=` path is resolved
   against the client's working directory (the DLL inherits the host cwd); set an
   absolute path for a fixed capture location.
4. The shipped `probe.cfg` targets the current investigation, edit it for a new one.
   Once the JSONL shows which register holds the buffer, resolve the central inbound
   dispatcher and outbound send chokepoint via Ghidra `:8089`, then add both as hooks
   in `probe.cfg` so two hooks capture all traffic.

Errors are written to `knc_probe.log` next to the DLL.

> **There is no safe live unload.** End a probe session by closing the client, the DLL
> intentionally skips teardown (unhook + thread join) during process termination to avoid
> deadlocking under the loader lock.

## Add a hook

Edit `probe.cfg`, no rebuild needed. `hook=<name> dir=in|out addr=0x...` then one or more
`capture=<label> src=<eax|ecx|edx|argN|esp+N> [deref=o1,o2] [size=N] [as=val|ptr]`.
`as=val` dumps the source value's low bytes (scalars like opcode); `as=ptr` (default)
treats the source as an address, follows `deref`, and dumps `size` bytes there.

## Read game state (polls)

A `poll=<name> interval_ms=N` block reads absolute globals on a timer instead of on a
packet. Its `capture=` lines MUST use `src=abs:0xADDR` (+ optional `deref`/`size`):
```
poll=player interval_ms=500
capture=mgr   src=abs:0x75b914 size=4              # *(0x75B914) = manager pointer
capture=local src=abs:0x75b914 deref=0,0x78 size=512   # g_mgr -> manager -> +0x78 = player
```
Records appear with `"dir":"state"`; read fields from the hex (e.g. id@0x120, form@0x156).
`interval_ms=0` = snapshot-only. Force a one-shot read by sending `snap` to the tap:
`echo snap | ncat 127.0.0.1 8077`. (`src=abs:ADDR size=4` with no `deref` reads the value
stored AT `ADDR`; `as=val` would dump the literal constant `ADDR`.)
`seq` is per-source (packet hooks and polls have independent sequence counters); order
records across sources by `t_us`.

## Dynamic breakpoints

Place breakpoints at runtime over the tap, they capture the full CPU context on hit
and (by default) **continue without freezing the client**. Commands (newline-delimited,
sent to the tap):

```
bp <hexaddr> [trace|suspend] [hw|int3]   # arm (defaults: trace, hw)
rmbp <hexaddr>                           # disarm
resume [<hexaddr>|all]                   # release a suspended BP
bplist                                   # list armed BPs (addr mode mech hits)
```
e.g. `printf 'bp 0x428390 trace hw\n' >/dev/tcp/127.0.0.1/8077`.

- **trace** (default): snapshot (`dir=bp` record: all regs + EIP/EFLAGS, a stack dump,
  the bytes at EIP) and continue immediately, no freeze. Inspect the snapshots.
- **suspend**: hold the hitting thread until `resume`, bounded by a ~30 s auto-resume
  timeout (so a forgotten hold can't kill the net thread). Only that thread waits.
- **hw** (default): hardware DR breakpoints, no code patch, **max 4**. **int3**: software
  `0xCC`, unlimited, for bulk/automation.
- On each hit the snapshot also annotates Ghidra `:8089` (a comment + bookmark at the
  address), deduped per address. Records go to the JSONL/tap with `"dir":"bp"`.

Caveats: HW BPs are armed on threads existing at arm time (a BP only ever hit by a
later-spawned thread is missed; int3 is global). `int3` is best-effort under heavy
multithreading, the original byte is out for the ~1-instruction step window, so a hit
on another thread during that window can be missed (never a hang/corruption). Don't BP
an address already inline-hooked by the packet engine. Suspend holds a thread, expect
a disconnect if the net thread is held past the server heartbeat.

## Reuse for another game

The engine is game-agnostic; only `probe.cfg` is KnC-specific. A new x86 game is
config-only. A 64-bit game needs an x64 build plus extending the `src=` grammar to
x64 registers (`rcx|rdx|r8|r9`), the ring/sink/config/JSONL layers are unchanged.
