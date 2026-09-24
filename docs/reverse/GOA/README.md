# GOA - Old KnC Client Reverse Engineering

Pre-release KnC client ("GOA" build), distinct from modern client in `docs/packets/`.

## Binary

| Field | Value |
|---|---|
| File | `KnC_dump_SCY.exe` (Scylla-unpacked) |
| Image base | `0x400000` |
| Arch | x86 (32-bit) |
| CRT | MSVC 7.1 (`msvcr71.dll`) |
| Engine | Gamebryo 1.2 |
| Internal title | `"Kart N' Crazy"` (14 xrefs) |
| Studio | Netamin |

## Files

| File | Contents |
|---|---|
| [NETWORK.md](NETWORK.md) | Socket stack, send/recv, dispatcher, anti-hook sentinels |
| [COMPARISON.md](COMPARISON.md) | GOA vs modern KnC delta |

## RE Method

IDA Pro against Scylla-dumped `.i64`. All `sub_4xxxxx` addresses are image-base-relative to `0x400000`.
