# Development Tools

Console and GUI tools that work with the client data tree, the kart physics port, and the network wire. Most live under `tools/` and build from the root CMake project.

## Build

```bash
cmake --preset vs2022
```

This writes `build/KnC.sln`. Build one tool with MSBuild:

```bash
scripts/build/find_msbuild.ps1
msbuild build/tools/<name>/<name>.vcxproj /p:Configuration=Release /p:Platform=x64
```

or build everything with the full build script in `scripts/build/build_sln.ps1`. CMake based tools land in `release/<name>.exe`. `clientpatch`, `headless`, `mitm` and `probe` are standalone projects outside the main solution, see their own section below for how to build them.

## col_tool

Dumps a track collision folder from the kart physics port. Prints, per piece, the cell count, edge count, and the two loaded but unused arrays, plus the count of cells per surface name.

Flags:
- `--at x z` prints the cell under that point, its three edge indices, its surface name and index, the ground height from the height plane, and the friction, grip and contact drag of that surface.
- `--cells` prints every cell, its index, its height plane A B C D, its edge indices and its surface name.
- `--regen` prints every row loaded from `regen.ini` in the track folder.
- `--surfaces` prints the friction, grip and contact drag of the nine known surface names (the drag is charged per loaded wheel, water 0.01, ice minus 0.001).

Build: `msbuild build/tools/col_tool/col_tool.vcxproj /p:Configuration=Release /p:Platform=x64`

Run: `release/col_tool.exe "Data/Public/World/Cookie/Cookie_01" --at -78.3 -8.521`

Location: `tools/col_tool/`, needs `KnCKartClient`, see `games/kart/physics/client/world_collision.h`.

## pak_tool

Console list, extract and pack tool for a pak dat archive. No window, no preview. The GUI `dat_manager` below packs through this same code.

The pack command writes the same entry layout the reader already decodes: a 12 byte entry header ending in a four byte file size, a 260 byte null padded path, then the raw file bytes. This round trips through `pak_tool` itself and through every other `PakReader` user in the tree, but it is not proven to match the original game packer byte for byte, since that packer format was never captured.

Build: `msbuild build/tools/pak_tool/pak_tool.vcxproj /p:Configuration=Release /p:Platform=x64`

Run:
```bash
release/pak_tool.exe list "DevClient"
release/pak_tool.exe extract "DevClient" out_folder
release/pak_tool.exe pack new_pak.dat input_folder
```

Location: `tools/pak_tool/`, uses `shared/src/PakReader.h` only, no engine dependency.

## dat_manager

GUI browser of the pak archives of a game folder, on bgfx, glfw and dear imgui, the stack of the ui editor. It opens `pak001.dat` and up through `PakReader`, lists every entry as a folder tree or a flat list with one colour per kind, filters by kind (All, IMG, NIF, SND), searches by path, and previews the selected entry:

- textures (DDS, PNG, BMP, JPG, TGA) decoded through the render module `TextureCache`, wheel zooms, drag pans, Fit and 1:1 buttons
- NIF models drawn by `SceneRenderer` inside the preview panel, lit by `Data/Public/World/Light.nif` of the pak, drag orbits, wheel zooms, R resets. A prop goes through `build_prop_model` with its particle systems, a skinned body through `build_character_model` with every clip its `<stem>.kfm` lists (every `<stem>_*.kf` of the folder when there is no KFM), the clip combo picks the one playing, Play, Pause and Restart drive the clock
- audio (WAV, MP3) decoded by miniaudio, Play and Stop, the cursor and the duration
- text (TXT, XML, INI, CSV, LUA, CAR, IFL) first 2 KB in a read only box, a UTF 16 file with a mark is converted, a KFM shows its sequence list

The details strip shows the name, path, size, kind, pak index and the preview facts (dimensions, parts, triangles, bones, clips, duration). Extract writes the selected file, or the selected folder tree, into a folder picked with a native dialog. Pack Folder writes a folder into a new pak file through the pak_tool pack code (`tools/dat_manager/pak_pack.cpp` compiles `tools/pak_tool/main.cpp`), so both write the same layout.

Textures of a model are found in the pak by name, the folder of the NIF first, then its subfolders (`BODYCOLOR`, `BODYSET`) with `High` before `Low`, then anywhere. The loaders read disk files, so the manager writes the entry and what it needs into a per process cache under the user temp folder, `knc_dat_manager_<pid>`, removed at exit. The game folder is never written.

The game folder comes from `--game`, else from `dat_manager.ini` (`[Settings] GameDir=`) in the working folder, else from a folder dialog, which saves it.

Keys and mouse: left drag orbits a model or pans an image, wheel zooms, R resets the orbit, Space plays or pauses the clip or the sound, Escape clears the selection.

Command line: `dat_manager [--game <client folder>] [--open <entry path>] [--screenshot out.png] [--frames N] [--size WxH] [--view yaw,pitch]`. `--view` sets the capture orbit in radians, `0,0` the front and `1.57,0` the side, so every driver can be checked upright and facing minus x. `--screenshot` draws N frames in a hidden window with the entry of `--open` selected and writes the PNG, the animation clock steps one sixtieth per frame there, so `--frames 120` shows two seconds of particles or clip. Checks: `release/dat_manager.exe --game <client> --open Data/Public/Car/Body/High/Basic_1/BODY.nif --screenshot kart.png --frames 30` draws the kart with its BLACK paint, `--open Data/Public/Driver/Body/High/Cosmo/body.nif` the driver on its idle clip with 11 clips in the combo, `--open Data/Public/Effect/Podium/winer_02.nif --frames 120` the podium with its confetti, `--open Data/Public/Car/Effect/particle_dust.nif --frames 120` the dust puffs. No track `Gimmick` NIF of the stock pak carries an emitter, and the room decorations under `World/Room/Effect` load with their one system but draw no particle standalone, the same as in `model_viewer`.

Build: `cmake --build build --config Release --target dat_manager`, or `msbuild build/tools/dat_manager/dat_manager.vcxproj /p:Configuration=Release /p:Platform=x64`

Run: `release/dat_manager.exe --game <client>`

Location: `tools/dat_manager/`, needs `KnCRender` and `glfw`, uses the dear imgui backend of `tools/ui_editor/imgui_bgfx.*`, the audio through `thirdparty/miniaudio`, the dialogs through `thirdparty/tinyfiledialogs`. Files: `dat_archive` (kinds, listing, cache, extract), `dat_preview` (the loaders and the camera), `dat_audio` (miniaudio), `dat_ui` (the imgui layout), `pak_pack` (the pak_tool pack), `main`.

Known gaps: OGG needs a vorbis decoder miniaudio does not ship, the stock pak holds none. A KF previews through its body NIF only. The first paint folder in name order colours a kart.

## NIF tools

Checks of the NIF and KFM readers against a client tree, no window.

- `nif_smoke <dir>` decodes every file and reports the blocks it cannot read
- `nif_rewrite <dir> <out dir>` reads and writes every file and diffs the bytes
- `nif_export <file.nif>` node tree and geometry as JSON
- `nif_animate <model.nif> <clip.kf> [frames]` baked skinned frames as JSON
- `kfm_smoke <dir>` reads every KFM

On the stock client data 2872 of 2880 files decode, the eight left are the `Public/Car/TestWheel` test assets at 10.0.1.0, and every decoded car file rewrites byte identical.

Build: `msbuild build/tools/nif/nif_smoke.vcxproj /p:Configuration=Release /p:Platform=x64` (each tool has its own vcxproj under `build/tools/nif/`)

Location: `tools/nif/`, needs `KnCFormats`.

## model_viewer

Opens one NIF with the new render module, lit by the `Light.nif` rig found above it. Drag to orbit, wheel to zoom, escape quits. `--screenshot out.png --frames N` draws N frames into a hidden window and writes the capture, `--textures dir` picks a texture folder, `--no-sun` shows the ambient only model.

A NIF with skin blocks, or any NIF given `--clip <anim.kf>`, is loaded as a character through `load_character_model` and drawn by the skinned shader, the first clip plays from the start. `--clip` may repeat, every sequence of every KF is bound on the skeleton by node name. Check of the loader: `release/model_viewer.exe <Cosmo>/body.nif --clip <Cosmo>/body_O_MAIN_MT_IDLE.kf --screenshot out.png --frames 30` with `<Cosmo>` at `Data/Public/Driver/Body/High/Cosmo`, the driver comes out seated with its textures.

Build: `msbuild build/tools/model_viewer/model_viewer.vcxproj /p:Configuration=Release /p:Platform=x64`

Run: `release/model_viewer.exe path/to/model.nif`

Location: `tools/model_viewer/`, needs `KnCRender` and `glfw`, see `docs/engine/RENDER_MODULE.md`.

## map_viewer

Opens one track folder with the new render module: `track.nif` on the terrain layer, the `Gimmick` NIFs as props, `sky.nif` and `sky_night.nif` as the day and night skies, the sun and ambient from `Data/Public/World/Light.nif`. On top it draws a collision overlay built from the `.col` cells (translucent blue, corners from the cell edges, height from the cell plane) and marker boxes for the `start.ini` rows (green, the long side points where the row drives) and the gimmick csv rows (magenta). Overlays draw through the track mesh so they stay visible. `geometry.nif` is not drawn by default: it is a helper mesh with one placeholder texture name `Geometry.BMP` that exists on no disk, and drawing it paints a white sheet over the road. `--geometry` shows it anyway.

With `--ghost <file>` the viewer plays a ghost recording (the `KCGR` file of `ghost_dump`, or raw 28 byte samples) and drives a car NIF along it, `Basic_1/BODY.nif` found above the track folder unless `--car <nif>` names another body. The pose comes from `GhostPlayback` so the blend is the client one. Position is used as is, the yaw byte turns the car with the client rule, forward is (-cos A, sin A) so yaw zero drives toward minus x, checked on a real recording of Race 01 where the first sample sits on start row 5 and the motion angle along the whole lap is 180 minus the yaw angle. `--chase` starts behind the car, `--seek <seconds>` starts later in the run.

The wheels come from four dummy nodes in the body NIF, `O_WHEEL01` to `O_WHEEL04`, plain translations under a `NiNode`, no rotation, each carrying only a tiny proxy box and a `NiTransformController` the viewer ignores. On `Basic_1` their z height matches the baked radius of the same numbered `WHEELn.nif` almost exactly: `O_WHEEL01`/`O_WHEEL02` sit at z 0.492 and `WHEEL1.nif`/`WHEEL2.nif` bake to a disc of radius 0.496 around their own origin, `O_WHEEL03`/`O_WHEEL04` sit at z 0.571 and `WHEEL3.nif`/`WHEEL4.nif` bake to radius 0.563, so each wheel just touches the ground at its own dummy, proof the file number matches the dummy number. `O_WHEEL01`/`O_WHEEL02` sit at x -1.221, just behind the headlamp dummies `O_LAMP01`/`O_LAMP02` at x -1.782, so that is the front axle, the smaller wheels; `O_WHEEL03`/`O_WHEEL04` at x 0.698 is the rear axle, the bigger wheels, next to the name plate and antenna dummies. Left and right follow the world rule yaw zero drives toward minus x so right is plus y: `O_WHEEL01` is front left, `O_WHEEL02` front right, `O_WHEEL03` rear left, `O_WHEEL04` rear right. The ported physics never sets a per wheel base, direction or scale (`games/kart/physics/client/body.cpp`, `body_create` and `body_load_wheel_config` copy the catalogue but never touch `CarBody::wheelObjects`, matching `BODY_MOTION.md`'s "no writer found" note on that same field), so the NIF dummies are the only concrete source for wheel placement, not the physics port.

The driver sits in the car. `--driver <body.nif>` names a driver body, `Data/Public/Driver/Body/High/Cosmo/body.nif` found above the track when the option is absent, `--no-driver` leaves the car empty. The body is loaded through `load_character_model` with every sequence its `body.kfm` lists (the `body_Anim.h` ids, 0 idle, 1 and 2 the drift loops, 3 back, 4 turbo, 5 crash, 6 damage, 7 and 8 item, 9 win, 10 lose, the right loop is sequence 1 of the two in its KF). Each frame the viewer picks the clip the client picks for a remote car in `car_visual_update` 0x48F515: back while the reverse bit is set, else turbo while a boost runs, else the drift loop of the lean side (flag bits 2 and 1, the steer keys) when the car moves over 3 units a second, else idle. The clip time restarts only when the clip changes, the loops loop by their KF cycle, there is no blend (the client blends 0.1 s). The drift state bits never pick a clip, the client keys the loops on the steer side. `--pose-test 1..4` forces left loop, right loop, back or turbo on a run that never drifts, the console prints every switch. The wheels follow the remote car rule of `car_visual_update` 0x48E7C4: the front pair eases to 30 degrees (0x5A6ACC) on the lean side by an eighth per 20 ms tick, the recording has no steer angle, and all four wheels roll by the distance driven along the car nose times 0.6 (the stock adds speed times minus 0.01 per frame at 60 frames a second). Rule and addresses in `docs/reverse/physics/EFFECTS_AND_GEAR.md`, Driver clips, and `docs/reverse/CLIENT_CAMERA.md`, The wheels.

The seat is not a dummy node of the car body. The client reads `Define/Driver/driver_pos_<driver>.ini` in `driver_seat_ini_read` 0x48C480 with `GetPrivateProfileStringA`, section is the chassis folder name, keys `x` `y` `z`, `driver_attach_to_car` 0x48D050 stores the three floats in the driver record and `driver_place_on_car` 0x48B800 sets the driver node every frame to `D3DXMatrixScaling(1, 1, 1)` times that translation times the car matrix, no rotation and no scale on the driver, a missing section fails the attach in the client and leaves the driver at the car origin here. For Cosmo on `Basic_1` the row is x -0.25 y 0 z 0.559: x sits between the axles (`O_WHEEL01` at -1.221, `O_WHEEL03` at 0.698, mid -0.26) and z 0.559 puts the Cosmo pelvis (0.247 above its own origin) at 0.8 above the ground, feet at 0.66 inside the nose. The `O_MAIN_MT` dummy of the body at x 0.305 z 1.154 was the other candidate because the clip files carry its name, it sits on top of the rear of the body and would float the driver above the seat, `O_MAIN_PK`, `O_MAIN_PS` and `O_MAIN_WC` sit at the same height, so none of them is the seat. The viewer prints the seat it used and where it came from.

Keys: WASD and space or control to fly, shift for speed, right drag to look, left drag to orbit, wheel to dolly, R resets the camera to the first start row, 1 to 5 toggle terrain, props, sky, collision and markers, F fog, G glow, escape quits. With a ghost: P pauses, C toggles the chase camera, Home restarts, comma and period seek two seconds back or forward, the window title shows the playback time.

`--orbit <degrees>` puts the camera on the garage preview circle around the ghost car, 5.2 out and 1.9 up, 0 behind the tail, 90 on its left side, 180 at the nose, for a capture of the kart from a chosen side. `--driver-offset x y z` moves the driver off its seat in car space, a depth test aid: with `-2.6 0 0` the driver stands beyond the nose and the hull must hide his body from the rear, with `2.6 0 0` he stands behind the tail and covers it, both captures are the proof the depth order of the character pass is right.

`--screenshot out.png --frames N --size WxH` draws into a hidden window and writes the capture, `--hour H` picks the day phase, `--no-sun`, `--no-col` and `--no-markers` skip the rig and the overlays.

Known gaps: the `Gimmick` NIFs sit at identity by proof, not by guess, the client names them per track id in `world_gimmick_load_by_track` 0x4D4180 and attaches them with their baked world transform (`docs/formats/FILE_FORMATS.md`, Track Gimmick NIFs), the viewer loads the same names, stands the three mushman and the six spiders on their exe tables, the drums on the `itemdrum.ini` rows with their yaw column, and logs every folder NIF no loader names, but the ant of Cookie 01 walks a `NiPathInterpolator` the render module does not play, so it rests at its pose, the driver clips cut with no blend, the one shots (crash, damage, item, win, lose) never play because the recording carries no such event, and the face texture swap by clip is not ported.

Build: `msbuild build/tools/map_viewer/map_viewer.vcxproj /p:Configuration=Release /p:Platform=x64`

Run: `release/map_viewer.exe "<client>/Data/Public/World/Race/Race_01" --ghost run.ghost --chase`

Location: `tools/map_viewer/`, needs `KnCTrackScene`, `KnCGhostReplay`, `KnCRender` and `glfw`.

## track_scene

`KnCTrackScene`, the library behind the map viewer. `load_track_scene(TrackSceneRequest, TrackScene&, error)` reads one track folder into a `MapScene` plus the raw `.col` track, the start rows, and the marker models and instances, so another tool can draw a track without repeating the loader. The col plane lives in the X Z ground plane with a height, the library remaps it into the renderer Z up frame.

`ghost_car.h` loads the ghost car body and its four wheels. `load_ghost_car(body_nif, GhostCar&, error)` loads the body NIF, finds the `O_WHEEL01` to `O_WHEEL04` dummy nodes in it, and loads the matching `WHEELn.nif` beside it into `GhostCar::wheels` with one placement matrix each in `GhostCar::wheel_local` and its radius in `GhostCar::wheel_radius` (half the z extent of the mesh), see the evidence in the map_viewer section above. `ghost_wheels_advance(car, car_world, moved, dt, turn_state, GhostWheelState&)` is the remote car rule of `car_visual_update` 0x48E7C4: every wheel rolls by the signed distance moved along the body's own forward axis times 0.6 (the stock adds speed times minus 0.01 per frame at 60 a second), and the front pair eases to plus or minus 30 degrees on the lean side by an eighth per 20 ms tick, the recording holds no steer value. `ghost_wheels_from_physics(car, spin[4], steer, GhostWheelState&)` takes the local car values of the physics port instead, `wheelsTick[i].spinAngle` (car 0x32A4, the body spin negated, a quarter of it off the ground) and the front `steerAngle` (the steer tangent times 6, twice in a drift, clamped 30 degrees). The angles are the D3DX ones of the client, `ghost_car_instances(GhostCar, first_model_index, car_world, wheels, out)` turns them into `bx::mtxRotateY(-spin)` and `bx::mtxRotateZ(-steer)`, appends the body instance then one instance per wheel, spin about the axle, steer on wheels 1 and 2 (the front axle), then the corner matrix, then `car_world`. The client camber (plus or minus 5 degrees per side) and the random bump of a wheel in the air are not ported. `find_texture` and `resolve_textures` walk the BODYCOLOR style folder beside a car NIF, shared by the body and every wheel, the `CharacterModel` overload does the same for a driver whose textures sit in `BODYSET`.

The driver: `load_ghost_driver(body_nif, chassis, GhostDriver&, error)` loads the body through `load_character_model` with every clip its KFM names (one `CharacterClipRequest` per sequence with the KFM id and the sequence position inside the KF), fills `GhostDriver::clip_of_sequence` and `idle_clip`, then `find_driver_seat(body_nif, chassis, seat)` reads `Define/Driver/driver_pos_<driver>.ini` above the body, section `[<chassis>]`, and `GhostDriver::seat_local` becomes that translation, identity when no row exists like the client. `ghost_driver_sequence(GhostPose, speed)` is the client rule of `car_visual_update` 0x48F515 for a remote car (back, turbo, lean side over 3 units a second, idle), `ghost_driver_clip(GhostDriver, GhostPose, speed)` turns it into a clip index with the idle clip as fallback, and `ghost_driver_instance(GhostDriver, model_index, car_world, clip, clip_seconds, out)` fills one `CharacterInstance` on that clip at that time with `seat_local` composed onto `car_world`, the translation only chain of `driver_place_on_car` 0x48B800, so the driver leans and slides with whatever matrix the caller gives as `car_world`.

Location: `tools/track_scene/`, needs `KnCRender` and `KnCKartClient`.

## replay

The ghost replay data path: the `KnCGhostReplay` library and the `ghost_dump` tool. A ghost is what a player uploads after a race, the server keeps it in `ghost_record` and `ghost_replay_chunk` (see `server/scripts/010_ghost_quest.sql`), 28 bytes per sample, ten physics ticks of 20 ms between samples.

- `ghost_dump <file> [--csv]` prints the header and every sample, index, x y z, yaw, flags, nibbles, input mask
- `ghost_dump --db <host> <port> <user> <password> <database> <track id> <char id> [--save <file>]` pulls one recording from a running server and saves it, only when the build found the MariaDB connector (`KNC_REPLAY_DB`)
- `GhostPlayback` in `ghost_replay.h` advances a recording in real seconds and returns the pose with the same 0.1 per tick blend as `car_ghost_sample_apply`, `seek`, `restart`, `finished`
- `ghost_compare <track folder> <ghost file> [--seconds N] [--centre] [--flags] [--edges] [--resync] [--resync-until T] [--trace] [--trace-from T] [--car path] [--stats 17 floats] [--tuning a b c]` drives the physics port with the recorded input mask and prints the gap to the recorded samples, the acceptance test of `games/kart/physics/client`. `--centre` applies each mask around its sample tick, `--flags` takes the turn side and boost bits of the status flags, `--edges` places each key edge inside its one sample window (the steer and the drift key by the recorded yaw and gauge, the gas and the brake by the recorded speed), `--resync` teleports the port back on the recording after each sample for the per sample error, `--resync-until T` stops that at T so a slice runs free, `--trace-from T` starts the per tick trace at T. The port speed of a row is the mean over the ten ticks like the recorded one, the end prints a class table, the drift key down, the two samples after a release, the brake, the gas alone. `ghost_compare <track folder> --script <file> [--start-row N] [--seconds N] [--trace]` drives the port with a key script instead of a ghost (lines of `time accel brake left right drift item`, the keys hold until the next line) and prints the drift state every 0.2 s, the drift work of `docs/reverse/physics/GHOST_REFERENCE.md` what a drift recording must show. The reference numbers and the runs are in `docs/reverse/physics/GHOST_REFERENCE.md`

The package database publishes no port on the host, so `--db` cannot reach it. Pull a recording with `docker exec knc-mariadb mariadb -u<user> -p<password> -N -B -e "select hex(data) from ghost_replay_chunk where track_id=90 and char_id=8 order by chunk_index" knc_emu`, concatenate the chunks, and write the `KCGR` header in front (a short python script does it, the layout is below). The user and password are `DB_USER` and `DB_PASSWORD` of the package `.env`.

Ghost file layout, little endian: the four bytes `KCGR`, u32 version 1, i32 track id, u32 char id, i32 time ms, u32 car kind, u32 frame count, 44 bytes char block, 56 bytes kart block, u16 name length, the name in UTF 8, then frame count samples of 28 bytes each, the same bytes as `ghost_replay_chunk.data`. A file without the magic whose size is a multiple of 28 loads as raw samples.

Build: `msbuild build/tools/replay/ghost_dump.vcxproj /p:Configuration=Release /p:Platform=x64`

Location: `tools/replay/`

## ui_editor

Visual editor for the client UI JSON layouts, on bgfx, glfw and dear imgui. The dear imgui sources come from the bgfx tree, the tool compiles them itself.

What it loads:

- the game folder, `pak001.dat` and up through `PakReader`, then the `Data/<Lang>/Image` and `Data/Public/Image` folders on disk, PNG, DDS and JPG decoded with `bimg`
- the screen files `ui_state_*.json` for the states Logo, Title, Login, Channel, Menu, Garage, Lobby, Room and Shop, searched in `Data/Public/UI/` or the folder given with `--ui-dir`, a hand made fallback fills a state with no file
- `ui_editor.ini` in the working folder keeps the game path, a folder dialog asks for it the first time

The window: main menu bar (File, Edit, View, language), the left sidebar with the state list, the element list with reorder arrows and the properties tabs Gen, Asset, State and Text, the canvas in the middle at 1024x768 game units with grid, bounds and corner handles, the tool strip on the right with one button per element type and the preview toggle, the status bar at the bottom. Two modal dialogs: Add New Element and the Asset Browser with search, folder filter and thumbnails.

Keys and mouse:

- left click selects and drags, the corner handles resize, middle drag pans, wheel zooms, Ctrl wheel changes the grid step
- Ctrl arrows move one pixel, Shift arrows resize one pixel
- Ctrl Z undo, Ctrl Y redo, Ctrl C copy, Ctrl V paste, Ctrl D duplicate, Delete removes, Escape deselects
- Ctrl S saves `ui_state_<state>_<Name>.json` in the working folder, P toggles the preview, Ctrl 0 resets zoom and pan

The written file keeps the client format, lowercase type names, `position`, `size`, `visible`, `enabled`, the button state assets and the input and checkbox properties. Export then reload then export gives the same bytes.

Command line: `ui_editor [--game <client folder>] [--ui-dir <json folder>] [--json <file>] [--state N] [--lang Eng] [--export out.json] [--screenshot out.png] [--frames N] [--size WxH]`. `--screenshot` draws N frames in a hidden window and writes the PNG, `--export` writes the current state and exits, both prove the tool without a display.

Build: `cmake --preset vs2022` then `cmake --build build --config Release --target ui_editor`, or `msbuild build/tools/ui_editor/ui_editor.vcxproj /p:Configuration=Release /p:Platform=x64`

Run: `release/ui_editor.exe --game <client>`

Location: `tools/ui_editor/`, needs the `bgfx` and `glfw` targets, split by feature: `ui_assets`, `ui_screens`, `ui_canvas`, `ui_sidebar`, `ui_dialogs`, `ui_json`, `imgui_bgfx` for the backend, `ui_screenshot` for the capture.

## network_decrypt

Standalone hex XOR decrypt and encrypt utility for the client `Network2.ini` config file. No engine dependency.

Build: `msbuild build/tools/network_decrypt/network_decrypt.vcxproj /p:Configuration=Release /p:Platform=x64`

Location: `tools/network_decrypt/`.

## clientpatch

A 32 bit `dinput8.dll` the client auto loads. Hooks the socket send and receive calls for live capture, carries a command pipe so a shell can drive the client, and patches the quest button re registration.

Build: `cmake -S tools/clientpatch -B build-clientpatch -A Win32` then `cmake --build build-clientpatch --config Release`, x86 only, WinAPI and gdiplus only, no engine dependency. Copy `dinput8.dll` and `knc-pilot.exe` next to `KnC.exe` (the dev copy is `DevClient/`, outside git) with `pilot.enabled = 1` and `pilot.d3d = 1` in `patches.ini`.

The pilot: `knc-pilot.exe <verb> [args]` talks to the pipe `\\.\pipe\knc_pilot` the DLL serves inside the client, one line in, one line out, `ok` or `err` first. Every verb but the ones marked pipe runs on the game thread between two frames.

| Verb | What it does |
|---|---|
| `dump` | stage, dialog, the selected char and kart, the owned counts, the cursor, the resolution and scale, the input gate, the window |
| `stage`, `waitstage N [ms]` | the stage int at `0x00B2360C`, the wait polls it off the game thread |
| `move X Y`, `click X Y`, `rclick X Y` | pins the cursor object at `0x00E52688` on the point then sends the mouse messages, client pixels |
| `key VK...` | a tap, `WM_KEYDOWN` `WM_CHAR` `WM_KEYUP` with the activate gate forced up for the send, names `enter esc space tab up down left right f1..f12` or a number |
| `text STRING` | `WM_CHAR` per character |
| `hold VK...`, `release VK...`, `holdmask`, `held` | the held keys, pipe thread, see below |
| `car` | the local car off the game object `0x01B19090`, pipe thread: `idx x y z yaw speed kmh drift gauge boost cp rank prog accel brake right left stuck held` |
| `peek ADDR [n]`, `poke ADDR HEX` | raw memory, `ADDR` a number or a name of the dump (`stage`, `inputgate`, `cursorx`, `game`, `localcar`, `inputmgr`, `asynckeyslot`) |
| `shot [path] [auto gdi d3d]` | a PNG of the client, `d3d` copies the back buffer on the next Present, the only clean capture of the window |
| `goto N` | calls `SetUIState` `0x00404410` with the stage index |
| `chars`, `karts`, `drivers`, `vehicles` | the owned lists and the catalogues |
| `d3ddiag`, `help` | the hook counters, the verb list |

The held keys. The stock client reads the keyboard in `input_manager_update_keys` `0x0044B4C0`, once a frame it calls `GetAsyncKeyState` for every virtual key through the import slot `0x0059F350` and stores a held byte at `0x005DEAF4 + vk` and an edge int at `0x005DEBF4 + vk * 4` (1 the frame the key goes down, 2 held, 3 the frame it goes up), the whole poll gated by the activate byte `0x00B23614`. The DLL patches that import in the exe at attach, the hook answers `0x8000` for every key of its held set and passes the rest to user32, so a held key is a real press for every reader, `input_key_down`, `input_key_pressed`, the drift update and the ghost recorder. `hold 38` keeps the up arrow down until `release 38`, `holdmask` drops every key, `held` prints the set, the hook counters and the client bytes for each held key so the poll can be checked. A hold forces the activate gate up and a keeper thread keeps it up while any key is held, so the client keeps polling when its window is not in front. Proof: `hold 38` in a race moves the car and `car` shows `accel=1` and the speed rising, the ghost recorder writes the mask `0x80`.

`tools/stock_drive.py` drives the stock client with the pipe from python, no exe spawn per command (0.2 ms a call). `walk` closes the welcome popup, double clicks the channel row, then garage, shop, mission menu, messenger and back to the lobby, every stage for `client_snap.ps1`. The channel row click, `enter_channel(name)`, also carries a fresh account through login: once the catalogues load with no owned character it picks the first driver of the `Create Driver` popup, types `name`, presses OK, then closes the `Driving Practice` welcome box and opens the Lobby tab, since an account with no finished tutorial lands there on every login until it is done. `room [log]` creates a room, picks the circuit track (track 90, the theme row of the choice popup scrolled five to the right, the fourth thumb) and retries START until a bot is seated and the race starts or 45 s pass, since the server seats its first bot 20 s after the room opens and the client refuses to start alone. `ghost [log] [ring]` opens the ghost menu, picks the circuit theme (four scrolls, the fifth thumb), starts and drives three laps. `race [log]` drives whatever race runs. The follower reads `car` at 20 Hz, aims at the next row of `follow_01.ini` of the track (the rows are `x y z yaw`, forward is minus cos A sin A), holds the gas, steers by the heading error, brakes over 70 degrees, backs out after 3 s stopped, and in `ghost` mode drifts on every turn the line bends over 25 degrees in three rows: the drift key with the turn side steer for 0.8 s, the key up, the gas lifted 0.12 s and pressed again, the mini turbo edge. `ring <file>` writes the local car ghost ring (car+0x3754, 28 bytes a sample, the count at car+0xA7858) as a `KCGR` file after a finish, the same bytes the client uploads, for a run the server did not keep since it stores the faster time only. Every screen point is a client pixel at 1024x768 in `POINT`. The client polls the real cursor so a click only lands with its window in front, `KNC_ALLOW_FOCUS=1` lets the script bring it there.

`tools/launcher_login.py <user> <password> [host] [port]` logs in like `KnCLauncher.exe` does (a `0xFE` frame with the two strings, the answer carries the token) and prints the session token, so the stock exe starts with `KnC.exe serviceid=1 userid=<user> token=<token>` and lands on the channel screen with no launcher.

Location: `tools/clientpatch/`, `tools/stock_drive.py`, `tools/launcher_login.py`.

## stock_docker

The stock client in a Linux container, 32 bit wine 10 on Debian trixie in Xvfb, wine's own d3d9 on mesa llvmpipe, driven by the clientpatch pilot. A script tests the emulator with the real 2014 client, no desktop and no hand on the mouse. The server stays on the Windows host (the package containers, login 50017, game 50018).

Run from the repo root in PowerShell. Two accounts are dedicated to this runner, `dock1` (password `dock1pwd`) and `dock2` (password `dock2pwd`), registered once through the web admin at `127.0.0.1:8080/register`, so a docker run never collides with the release gate, which drives `hltest` and `hltest2`. `-User` defaults to `dock1` and falls back to `-FallbackUser dock2` right away, with no wait, when `dock1` answers busy.

```
powershell -NoProfile -File tools/stock_docker/run.ps1 -Scenario walk
```

| Scenario | What it proves |
|---|---|
| `boot` | the exe starts under wine, the pilot answers `dump`, shots of the logo, the title and the channel screen |
| `lobby` | `boot`, then the welcome popup, the channel row double click (a fresh account creates its driver and clears the licence welcome box) and the lobby with the char and kart on the stand |
| `walk` | `lobby`, then `stock_drive.py walk`: garage, shop, mission menu, messenger and back to the lobby, a shot per stage |
| `race` | `lobby`, a room on the circuit track (track 90), START until a server bot is seated, the `stock_drive.py` follower drives, the race ends, the result board, the room, the lobby |
| `refs` | `lobby`, then the capture plan of `docs/client/UI_PARITY.md` in one login: menu gear popups, shop tabs, gacha, garage equip, pendant, room craft, car factory, licence, messenger and mission, one shot per clone pair name into `reference/docker/<run>/` |
| `factory` | `lobby`, then the car factory: the top row, the second slot and its tire list, the cover, tire and booster lists, install, remove, the part popup, chassis and tire, Save, the name plate click and the rename, then the garage Car tab with the first two karts and their detail box. A runtime error box ends the run as a fail with `client_stack.txt`, every thread backtrace off `winedbg` |
| `paint` | `lobby`, then the garage Car category, Paint tab, the first cell and the button at the Remove spot (Delete on an expired row), fails on a box after it |
| `shell` | Xvfb and the port forwarders up, no client, an interactive bash for a hand test |

Parameters: `-User` and `-Pass` (default `dock1` and `dock1pwd`), `-FallbackUser` and `-FallbackPass` (default `dock2` and `dock2pwd`, tried once with no wait before the retry wait applies), `-Client` (default `DevClient`, a path under `KNC_PROTECTED_CLIENT` is refused), `-Out` (default `reference/docker`, git ignored), `-NoBuild`, `-Retries 3` and `-RetryWait 45` (a new try when the server says the account is in game), `-WineOverrides` (replaces `WINEDLLOVERRIDES`), `-Desktop`, `-CacheVolume` (default `knc-stock-cache`), `-DbContainer` (default `knc-mariadb`, clears the licence tutorial flag, see below), `-Network` (a docker network to join, so `-ServerHost` can name a throwaway server container on it, the login and game servers then share one network namespace since the forwarders send 50017 and 50018 to the same host).

Each run writes `reference/docker/<run id>_<scenario>/`: the shots `NN_<label>.png`, `run.log`, `scenario.log`, `summary.json` (result, marks in seconds from the container start, fps, every shot with its stage and capture method, laps, finish), `race_drive.log` for a race, and `logs/` with `wine.log`, the DLL `packets.log` and `clientpatch.log`, the bridge, Xvfb and socat logs. The runner prints the marks, the fps and the shot list. Exit code 0 pass, 1 fail, 5 the account was in game on every try, 6 a fresh account got stuck on the licence tutorial gate (see below), cleared and retried once automatically.

How it works:

- The image (`Dockerfile`, context `tools/`) builds `pilot_bridge.exe` with mingw, installs wine32 with its GL and ALSA libraries, and bakes a win32 prefix with native vcrun2022 (the pilot DLL is a VS 2022 build). A cold build takes about 10 minutes and downloads vcrun2022 from Microsoft, the image is 3.2 GB.
- `DevClient` is mounted read only on `/client-ro`. Small files are copied into `/game`. `Data` and every file over 1 MB go once into the named volume `knc-stock-cache` and are symlinked. `run.ps1` stamps the `Data` tree on Windows (file count, bytes, newest write, 2 s) and the container copies it again only when the stamp changes. The first copy takes about 3 minutes, then the tree is ready in 2 s. Read over the Docker Desktop mount, the stock boot blocks its game thread for up to 2 minutes and the lobby for 15 s.
- socat forwards `127.0.0.1:50017` and `127.0.0.1:50018` in the container to `host.docker.internal`, since the client and the `0x54` redirect both dial `127.0.0.1`.
- `launcher_login.py` gets the token and the entrypoint starts `wine KnC.exe serviceid=1 userid=<user> token=<token>`.
- Linux python cannot open a wine named pipe. `pilot_bridge.exe` runs in the same prefix, listens on `127.0.0.1:47017` in the container, one line in and one line out, one pipe transaction per line. Before `move`, `click` and `rclick` it moves the real cursor to the same client point. It sends a game thread verb only while the `calls=` counter of `held` grows, so only while the game thread runs frames, see the pilot bug below. Its own verbs: `bridge rect` (the client area on the X screen) and `bridge warp X Y`.
- `scenario.py` imports `tools/stock_drive.py` unchanged and swaps its `pilot` call for the bridge, so `walk` and the follower are the same code as on Windows. A shot is `shot <path> d3d` first, the back buffer copy works under wine. A flat frame (a fade) or no Present in time falls back to an X grab of the client area.
- `WINEDLLOVERRIDES`: `dinput8=n,b` (the pilot DLL), `d3d9=b` (the folder ships a dgVoodoo `d3d9.dll`, wine's own is used), `msvcr71,msvcp71=n,b` (the runtime shipped in the folder, see below).

Timings with a warm cache on a 12 core host: channel screen at 25 to 30 s, lobby at 40 to 50 s, `lobby` 63 s, `walk` about 100 s, `race` about 5.5 minutes. Frame rate on llvmpipe: menus 35 to 50 fps, race 11 to 15 fps, down to 5 fps when the host is busy. Everything seen renders: the 3D stand, garage, shop tiles, mission menu, messenger, room, the race track with its HUD, the podium. No sound, ALSA is a null device.

Known limits and findings:

- One account, one game. When another tool drives the same account, the server answers "That account is already logged in." over the title, the scenario sees the box after 20 s, exits 5 and the runner tries again. Use an account nothing else drives.
- The server seats its first bot 20 s after the room opens and the stock client refuses a start alone ("you need at least 1 other member"). Both the `race` scenario and `stock_drive.py room` close that box and press START again, until the race starts or the wait runs out (75 s in the scenario, 45 s in `stock_drive.py`).
- A fresh `dock1` or `dock2` login shows the `Create Driver` popup once the catalogues load. `enter_channel` in `stock_drive.py` picks the first driver, types the account name and presses OK. The new character then lands on the `Driving Practice` licence tab with a welcome box, and every account with `characters.tutorial_completed = 0` lands there on every login after too. No click, tab or button reaches the Lobby from that tab, proven by clicking it directly over the bridge with no stage change. `enter_channel` closes the welcome box, tries the Lobby tab a few times, then gives up within about 10 s and reports `stuck on stage 14`, rather than spinning for minutes. `scenario.py` turns that into exit code 6, and `run.ps1`'s `Clear-TutorialGate` runs an `UPDATE` through `docker exec knc-mariadb mariadb` (reading `MYSQL_USER` `MYSQL_PASSWORD` `MYSQL_DATABASE` off the container) before the first attempt and again on a 6, then retries. A brand new account so needs two attempts the first time it is ever used, about 60 s then about 50 s; every run after that is a single clean pass since the flags stay set. `-DbContainer` names the mariadb container, default `knc-mariadb`.
- Clearing only `tutorial_completed` is not enough. A real player only reaches it by passing a licence test, which also raises `characters.license_class` above 0 (`server/scripts/013_kart_part_wire.sql` links the two the same way for old rows). With `tutorial_completed = 1` and `license_class` left at 0, the lobby reaches stage 8 fine but Garage, Shop, Mission, Ghost and Quest and even a second CREATE all take the click and stay on stage 8, proven live over the bridge on the same account across many tries with no change and no dialog. `Clear-TutorialGate` sets `license_class = GREATEST(license_class, 1)` in the same statement so the two never drift apart again.
- The follower laps in 85 to 120 s under llvmpipe and the server bot wins in about 155 s, so the rest time usually retires the local car. The pass rule is one lap of the local car, then the room after the race, then the lobby.
- The slow frame rate sends motion in bursts. The game server logs "motion sample out of order", speed spikes over the ceiling and "drift boost violation" for a container racer, with no kick. Keep that in mind when reading the race log of a container run.
- Wine 10 builtin `msvcr71` `fscanf` returns 0, not EOF, when only `\r\n` is left. `gimmick_load_itemdrum` (`0x0048AF20`) loops `"%f,%f,%f,%f\r\n"` until EOF over a three column `itemdrum.ini`, counts a fifth junk drum, `itemdrum_place_row` finds no ground under it and the race stops on the modal box "Item initialize fail !". The native `msvcr71.dll` of the folder counts four rows like Windows, hence the override.
- Pilot bug, fixed, `tools/clientpatch/pilot.cpp` `dispatch()`: the `Command` used to live on the pipe thread stack. When the game thread took over 20 s (a long load) the wait timed out, the pipe thread returned `err timeout game thread busy` and the dead `Command` stayed in `g_queue`. The game thread later ran `execute` on it and faulted in `pilot::split` (seen twice under wine, "Unhandled page fault ... at address 794C7711", RVA `0x17711`). `g_queue` now holds a `std::shared_ptr<Command>`, so a timeout cannot dangle it: a timeout marks the command abandoned, the game thread skips an abandoned command it has not started and just finishes one already running. `pilot_bridge.c`'s `game_thread_live` check still gates game thread verbs on the `held` call counter growing, kept as a second guard so a verb does not wait out a long load.
- The stock message boxes carry the window title "Chibi Kart", the bridge picks the largest window with that title. The entrypoint copies a wine fault line and the last `[MSG ] box` lines of the spy log into `run.log`.

Location: `tools/stock_docker/` (`Dockerfile`, `entrypoint.sh`, `scenario.py`, `pilot_bridge.c`, `run.ps1`, `prefix.reg`, `asoundrc`).

## headless

A no window client. Logs in, walks the login to lobby flow, decodes the catalogs, and logs the full two way wire, for diffing server builds or as a scriptable netcode client.

Build: its own `CMakeLists.txt` under `tools/headless/`, links the netcode library `client/net` (`KnCClientNet`: NetClient, Packet, CatalogDecode, Session) the client uses, no engine or RHI dependency. `cmake -S tools/headless -B build-headless -A x64` then `cmake --build build-headless --config Release`, the exe lands in `build-headless/Release/`.

Location: `tools/headless/`.

## client script harness

The client itself is the scriptable UI test of the new stack. `release/knc_client.exe --script <file>` reads one verb per line and clicks, types and takes pictures like a hand on the mouse, through the same input entry points the glfw callbacks use, so a script proves the real path of a button and a screen. Verbs: `wait N` frames, `waitfor SCREEN [N]`, `click X Y` and `rclick X Y` in the 1024 by 768 stock coordinates (the press this frame, the release on the next), `key NAME` (`escape`, `enter`, `f1`, a letter, a glfw code), `text WORDS` as char events, `shot NAME` a png next to the `--screenshot` path, `expect SCREEN` prints PASS or FAIL with the top screen name, `quit`. The exit code is 1 when an `expect` failed. With `--screenshot` the window is hidden and 25 frames make a second.

```
release\knc_client.exe --game <client> --host 127.0.0.1 --port 50017 --user hltest --pass hltestpw --no-focus --mute --debug --stop-at lobby --script s.txt --screenshot out.png
```

Location: `client/app/App.cpp` (`runScript`), the verbs and the popup names in `docs/client/README.md`.

## mitm

A transparent relay between the client and the server on port 50017. Sees outbound traffic that a socket hook inside the client process cannot, and can inject a direct login past a dead launcher token.

Build: its own `CMakeLists.txt` under `tools/mitm/`, plus python scripts for offline capture parsing.

Location: `tools/mitm/`.

## probe

An injected 32 bit DLL that hooks the client message path, streams parsed packets and memory snapshots to a log file, and supports live breakpoints over a network tap. Ported from another game project, config only per title.

Build: its own `CMakeLists.txt` under `tools/probe/`, x86 only, bundled hook library, has its own test suite.

Location: `tools/probe/`, see its own readme for the config format and the breakpoint protocol.

## proxy_dll

A DLL proxy for the client engine module that logs every class instantiation call. Single file build script, no CMake.

Location: `tools/proxy_dll/`.

## Opcode audits

Three python scripts at the root of `tools/`, run with python3, no build step.

- `dispatch_audit.py` checks every opcode constant comment against the client real dispatch table
- `opcode_audit.py` enforces one name per opcode across the protocol header and the generated packet headers
- `opcode_truncation.py` catches packet constructor calls that silently truncate an opcode past 255
- `opcode_pages.py index` rebuilds `docs/packets/opcodes/README.md` and the two tables of `PACKET_REGISTRY.md` from the opcode files, `check` verifies every file has its header row, `split` was the one time cut of the old registry

## Status

Done: NIF tools, model viewer and map viewer on the new render module with the ghost replay playback, the track scene library, collision dump tool, pak tool, dat manager, ghost dump, network decrypt, ui editor, clientpatch, headless, the client script harness, mitm, probe. Every tool in this repository builds on the bgfx render module or on the shared library alone, none of them need the old RHI based engine.
