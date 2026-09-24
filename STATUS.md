# Project Status

**Updated:** 2026-09-23

---

## Components

### Engine
**Status:** Working, the bgfx render stack only

- bgfx renderer and its own NIF reader, `engine/render` and `engine/formats`, imported from the X-Legend editor on 2026-09-14
  - Draws a KnC car and a KnC track through `tools/model_viewer` and `tools/map_viewer`, with the client's own light rig read from `Light.nif`, right handed projection so the banner texts read the right way
  - Base of the client rewrite, see `docs/engine/RENDER_MODULE.md`
  - The old RHI based engine that came before it is not part of this repository
- Client physics reverse: complete, `docs/reverse/CLIENT_PHYSICS_MAP.md` and eleven files under `docs/reverse/physics/`, eight passes read on the bytes
  - Mapped end to end: the tick, the drift update, the body layer, the collision file and its BSP, the height plane, every tick helper, the remote car, the input, the stats and the track record from the wire, the effects, the gear and tyre model, the respawn
  - The map keeps the open questions that the bytes could not settle, none blocks the port
- 1:1 port of the client's own physics: `games/kart/physics/client`, every module in, no `TODO open` marker left, nine tests, compared against a real ghost recording of Race 01 through `ghost_compare` (`docs/reverse/physics/GHOST_REFERENCE.md`)
  - World collision, the rigid body from the `.car` catalogue with the tyre model and the interleaved RK4 at the client's 1.5 ms step, stats, input, gimmicks, remote car, the motion packet and the tick with drift, boosts, effects and respawn are in
  - Against the recording: launch curve within one unit, first turn within a degree, yaw within two degrees over 16 s with the key edges placed, per sample prediction error 0.33 units mean over the three laps, bank included
  - Left: the yaw byte truncation and the one tick key edge window bound what a ghost can prove, the residual sits inside them, `docs/reverse/CLIENT_PHYSICS_MAP.md` open questions

### Server
**Status:** Working with the stock client, tested every day on it

- Login by launcher token or by credentials, ticket handoff to the game server, one account online at a time
- Character creation, licence tests, tutorial flag
- Lobby, channels, rooms, quick match, room craft decor in the waiting room
- Item and speed races, standings, finish, rewards, race history
- CPU cars on the racing line, items, hits, rubber band, rest counter after the first finisher
- Shop with every catalogue on sale, three price rows, purchase, gift, extend
- Gacha, pets, car craft, quests, missions, ghosts, notes, friends, clans in chat
- Web admin and signup page
- Chat commands for a local server, GM commands
- Shipped as the release package: server image, launcher script, the clone exe, Docker

Open: kart durability and repair items, team modes and battle mode need testing with CPU cars, the Advanced licence needs level 10 as the client demands, some tutorial screens miss art

### Client rewrite
**Status:** Phase 3b in, character creation, missions, messenger, whisper, room password, teams, sound and a two human race, proven on the package server

- `client/` on glfw and bgfx, target `knc_client`, guarded on `KnCRender` and `glfw` like the tools, `docs/client/README.md`, the plan in `docs/client/CLIENT_REWRITE_PLAN.md`
- The netcode is the library `client/net` (`KnCClientNet`: NetClient, Packet, CatalogDecode, Catalog, Session), `tools/headless` links it, both build, the headless reaches the lobby as before. Catalog keeps the `0x00BF` `0x00C0` `0x00C1` `0x00C2` `0x00C3` `0x00C4` `0x00C6` `0x0103` rows with their price options and the `0x001B` `0x001C` `0x001D` `0x001E` owned records in full
- Sprite batch on bgfx with rotated and free quads, Arial regular and bold from the windows font folder through stb truetype as the stock GDI font table draws them, the `ui_state_*.json` screens drawn from the pak images through bimg at the spots of the decompiled stock draw code, the scene renderer boots bgfx so a 3D screen draws under the sprite pass
- The look matches the stock client screen by screen, `docs/client/UI_PARITY.md`: the stock menu frame with the wallpaper and the common back behind the lobby, the garage, the shop and the mission menu, the char info column, the room cards, the transparent panels drawn with nothing under them, the race HUD at the stock spots with the turned needle, the minimap through the stock camera model of `minimap.nif` and `minimap.ini`, the standings with the faces and the ping bars, the lap words, the result board art, the escape menu, the shop with the 3D preview and the ShopItem popup, the waiting room with the karts in 3D
- `--state <name>` opens any screen offline on a loopback sample server that speaks the wire, F12 writes the frame next to the exe, `tools/client_snap.ps1` captures the stock client window per stage without touching its focus, the three together let the user compare a state in both games
- Logo, login, channel, menu and lobby screens: login `0x0007`, channel list `0x000E`, `0x0018`, handoff `0x0054`, reauth `0x00A7`, refresh `0x000A`, lobby ack `0x0012`, room rows `0x002D` `0x002E` `0x0023` `0x0031`, chat `0x00B4`, keepalive `0x00A6`, ping `0x000B`, a row click joins with `0x002F`, the create box sends `0x002D`
- Room screen: `0x0013` `0x0032` `0x0021` `0x0030` `0x0033` `0x0034` `0x0035` `0x0022`, the track pick over the `0x00C3` rows, ready and start on `0x0033`, exit on `0x0012`
- Race: the track scene of `tools/track_scene`, the local kart on `games/kart/physics/client` set up as the ghost harness, the other karts on `0x0040`, `0x000D` scene loaded, `0x003A` GO, a ten second countdown, `0x0040` every 100 ms, `0x0067`, `0x0041` on the COL faces, items `0x0049` `0x0058` `0x00CF` `0x0047`, `0x0045` standings, `0x0044`, `0x003C` answered with `0x0058` 9, `0x003D` `0x0039` `0x0046` `0x0042`, the room `0x0013` the server resends
- Garage: `0x000F`, the owned lists on the four tabs of `ui_state_06_garage.json`, the sub tabs by equip slot as `shop_part_tab_filter` 0x418C80 maps them, Equip `0x00B9` and Remove `0x00BA` with the category of the page and the base key, `0x00B9` and `0x00BC` back, the durability bar from the owned kart `+0x30`, the four stat bars as `garage_stat_bars_compute` 0x428AB0, the selected kart and driver in 3D on the empty scene of `RaceView` with the seat of `driver_pos_<driver>.ini`
- Shop: `0x0010`, drivers karts parts items and pets on the tabs of `ui_state_13_shop.json` filtered on `visible_flag` and `required_level` with the badge overlay, the price options joined with the `0x00C6` rows, the detail panel on `Define/Eng/def_trans_index.txt` and `def_trans_message.txt` line to line, Buy `0x00B7` with the price key, the wallet from `0x00B7` back
- `--auto-race <track>` drives the line by itself and captures the room, the countdown, the race and the result, `--wait N` lets the server seat its CPU cars, `--auto-create` and `--auto-join` cover the two sides of a room, `--stop-at garage` and `--stop-at shop` walk the two stages with a capture per tab, `--auto-buy` buys the cheapest consumable and captures the wallet before and after
- On the package server the port drives Race 01 in three laps of about 43 s and Forest 01 in laps of about 35 s, with two CPU cars the standings, the effects on the bots and the board rows all land
- On the package server with `admin`: the garage walk reads both `0x00B9` acks and two `0x00BC`, the shop buy takes the wallet from 7894 to 7394 gold on screen and in the database, the item lands in the garage Item tab
- Character creation: `0x0003` opens the RegistDriver popup on the `0x00BF` rows with the creation pick flag, `0x0004` with the driver key and the nickname, the result rule of the page, two fresh accounts got their characters through it, the `0x0016` licence ack after it is answered with `0x0012`
- Missions: `0x008F`, `0x0087` defs, `0x0088` and `0x008A` progress, the menu on the mission art, `0x0090` start, the run on `World/Mission/<world_name>` with the own kart, `0x0120` path points reported with `0x0121`, `0x008C` on the goal and its answer, mission 2 driven to the end on the package
- Messenger: `0x0076` `0x0078` `0x0079` `0x0073` `0x0077` `0x006F` `0x0070` `0x0071` `0x0074` `0x0081` `0x0082` `0x0083` `0x0084` `0x0085`, the two test characters made friends and exchanged a note through it, the whisper as `/w name text` on `0x00B4` and `0x00B5` back, `0x0126` system lines, the room password on `0x002D` and `0x002F` with the password box, teams on `0x0064` in the two team modes
- Sound on miniaudio, `client/assets/SoundBank`, the stock wav loops per screen and the theme loop of a race, the click, the countdown cues, the engine loop pitched by the speed, the item and lap cues, `--mute` for the headless runs
- Two human race on the package: `hltest` hosts, `hltest2` joins, both boards list the two names with their times
- Not in: pets, gacha, room craft, car craft, sell, extend, the repair scroll, the paint on the 3D body, the licence screen, the block tab, the room invite popup. Quick match `0x0064` from the lobby gets a `0x0063` and no room context from our server

Rule: the server is the reference. The client adapts to the server, never the reverse.

Open: the stock captures of every screen for `UI_PARITY.md`, the room craft world behind the waiting room, the channel user list of the lobby, the licence screen, the quick match room context on the server side

### Tools
**Status:** Working

- Headless client: logs in, races, drives a line, replays ghosts, used to test the server without the game, on the `client/net` library
- Client patch dll: packet spy, working directory fix, extra buttons
- MITM proxy and packet inspector
- PAK tool, collision dump, ghost replay reader and dump
- Model viewer and map viewer on the new render stack, the map viewer plays a ghost recording over the track with a chase camera
- UI editor on the new render stack, bgfx and dear imgui, headless screenshot and export
- `tools/nif` reader checks

---

## Known Issues

- Team modes and battle mode are untested with CPU cars
- The physics port residual against a ghost sits inside the yaw byte and the key edge window, a finer capture would be needed to go further
- Some tutorial screens miss art
- Pre-existing linker: `openssl/err.h` not found (web-admin project)

---

## Build

```bash
cmake --preset vs2022
cmake --build --preset vs2022
```

Requires: Windows, Visual Studio 2022, CMake 3.21+. Servers are their own CMake project, see `BUILD.md`.
