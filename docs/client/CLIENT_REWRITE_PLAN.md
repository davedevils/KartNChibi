# Client rewrite, the plan

The goal is a client of our own that plays on our server: login, channel, lobby, room, race, garage, shop. It is built from bricks that exist and work, nothing is invented twice.

## The bricks

| Need | Brick | Where |
|---|---|---|
| Window, input, frame loop | glfw plus bgfx as the viewers do it | `tools/map_viewer/main.cpp` init pattern |
| 3D, track, cars, driver | `KnCRender`, `KnCTrackScene`, `ghost_car.h` | `engine/render`, `tools/track_scene` |
| Physics of the local car | the 1:1 port | `games/kart/physics/client`, `tick.h` |
| Remote cars, motion packet | `remote_car.h`, `motion_packet.h` | same folder |
| Wire, login, redirect, catalogs, room, race flow | the headless netcode | `tools/headless` (NetClient, Packet, CatalogDecode, Drive, the Flow of main.cpp) |
| UI layouts | the `ui_state_*.json` screens and the pak images | `Data/Public/UI`, `engine/ui/UILoader.h` for the JSON shape, `tools/ui_editor` for how they draw |
| Text | stb truetype atlas, the bgfx imgui backend shows how | `tools/ui_editor/imgui_bgfx.cpp` |
| Sound | miniaudio | `thirdparty/miniaudio` |
| Packets | one file per opcode | `docs/packets/opcodes/` |

The server is the reference. The client adapts to the server, never the reverse.

## Layout

```
client/
  CMakeLists.txt        target knc_client, guarded like the tools on KnCRender and glfw
  app/                  main, the frame loop, the screen stack, the options
  net/                  the netcode library moved out of tools/headless, the headless links it too
  ui/                   sprite batch on bgfx, font atlas, the JSON screen loader, widgets, the screen base
  screens/              logo, login, channel, menu, lobby, room, race, result, garage, shop
  race/                 the race session, local car on the port, remote cars, HUD, camera
  assets/               pak and Data lookup, texture cache, sound
```

## Phases

1. Skeleton and the way in. Window, sprite batch, font, the JSON screens drawn from the pak images, login on the real server, channel list, redirect, the lobby with its real room list. Options `--host --port --user --pass --screenshot --frames --auto` so a capture proves each screen headless. The netcode moves to `client/net` as a library that `tools/headless` links, both keep building.
2. Room and race. Create and join a room, ready, start, the track scene, the local car on the physics port with the keyboard, remote cars from 0x40, the motion report every 100 ms, items, laps, finish, the result screen. The headless race flow (`Drive.cpp`, the host and join modes) is the reference for the wire order.
3. Garage and shop. Kart and driver select, parts, the shop tabs, gacha, missions, the notes and friends panels, sound.

Each phase ends with captures of every screen on the package server and a note in `STATUS.md`.

## Rules

- Comments one line, at most 20 words, only letters digits space @ % - " '. No em dash anywhere.
- Namespace `KnC::Client`. C++20 like the render module.
- No dependency on the old RHI stack, not part of this repository.
- Every packet the client sends or reads is named by its opcode file under `docs/packets/opcodes/`, a field the doc does not know is a doc bug to report, not a guess to ship.
