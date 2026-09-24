# Client shading

KnC.exe, 32 bit, image base 0x400000. Date of this pass: 2026-09-14.
Covers the world light file, the lights active during a race, the material and vertex colour render state path, texture stage state, fog, and the car specific body colour and shader lookup.

## The light file

`light_file_load` (renamed from `FUN_00443c10`, 0x443c10) is a small loader: `sprintf(acStack_104, "%s.nif", param_2)` then a virtual call `(**puVar1)(puVar1, acStack_104)` on an inner object at `param_1[1]`, and on success sets a byte flag at `param_1+8`. It does not parse anything itself, it hands a `.nif` path to a Gamebryo loader object.

`world_track_init` (0x4875c0) calls it at 0x4876fa: `sprintf(local_204,"./Data/Public/World/light"); cVar1 = light_file_load(local_204);` (this-pointer implicit in ECX). The string `./Data/Public/World/light` lives at 0x5a6208 and is referenced from 5 places: `world_track_init` (0x4875c0), and `FUN_00487a90`, `FUN_00488000`, `FUN_00488300`, `FUN_00488d60` (not decompiled, likely other world/menu init variants that load the same fixed light rig). So every world screen loads the same one file, `./Data/Public/World/light.nif`.

On disk this is `Data/Public/World/Light.nif`, 764 bytes. Read and hand parsed (Gamebryo NIF, no existing tool for this repo's engine was reused, done directly against the bytes):

- header: `"Gamebryo File Format, Version 10.2.0.0\n"`, version u32 @0x27 = 0x0A020000, user version u32 @0x2B = 0, num blocks u32 @0x2F = 7, num block types u16 @0x33 = 5.
- block types: `NiNode`, `NiZBufferProperty`, `NiVertexColorProperty`, `NiDirectionalLight`, `NiAmbientLight`.
- 7 blocks, in order: NiNode "Scene Root" (0), NiZBufferProperty (1), NiVertexColorProperty (2), NiNode "Direct01" (3), NiDirectionalLight (4), NiNode "Direct01.Target" (5), NiAmbientLight (6).
- Scene Root: identity transform, properties = [VertexColorProperty, ZBufferProperty], children = [Direct01, Direct01.Target, AmbientLight], effects = [DirectionalLight, AmbientLight]. In Gamebryo a light only actually lights a subtree if it is reachable through a node's effects list, which both lights are here (direct children of the world root, so both effects apply to everything under the root).
- NiZBufferProperty: flags u16 @0x12B = 0x0003, function u32 @0x12D = 3 (Gamebryo `ZCOMP_LESSEQUAL`, guess on the name, the ordinal is read).
- NiVertexColorProperty: flags u16 @0x13D = 0x0000, vertex mode u32 @0x13F = 0, lighting mode u32 @0x143 = 1. The binary's own class dump table (see Material section) shows lighting mode 1 = `LIGHTING_E_A_D`, i.e. `LIGHTING_EMISSIVE_AMBIENT_DIFFUSE`. Vertex mode 0 is read but no dump string for it was found in this binary; by the standard Gamebryo enum this is `SRC_IGNORE` (guess on the name only, the value 0 is proven).
- NiNode "Direct01": translation @0x15D = (-12336.216796875, -1002.2261962890625, 20311.640625). Rotation @0x169 (row major 3x3) = (0.4849086, 0.7208151, -0.4952668, -0.8745648, 0.3996610, -0.2746042, -1.4901161e-08, 0.5663008, 0.8241986). Child = NiDirectionalLight.
- NiDirectionalLight: local translation (0,0,0), local rotation @0x1BF = (0,1,0, 0,0,-1, -1,0,0) (an axis remap, not a rotation by angle). Switch state byte @0x1EF = 1 (on). Dimmer float @0x1F4 = 1.0. Ambient @0x1F8 = (0,0,0). Diffuse @0x204 = (1.0, 0.964706, 0.921569) = RGB (255, 246, 235), a warm near-white. Specular @0x210 = same as diffuse.
- NiNode "Direct01.Target": translation @0x239 = (0.132080078125, 5837.75, -217.9013671875). This is a sibling of Direct01 under Scene Root, not its child, so this is a world space point, the classic 3ds Max "target directional light" look-at helper.
- NiAmbientLight: switch byte @0x2C7 = 1. Dimmer @0x2CC = 1.0. Ambient @0x2D0 = (0.784314, 0.784314, 0.784314) = RGB (200, 200, 200), a mid grey. Diffuse @0x2DC and specular @0x2E8 are both (0,0,0).
- Trailing header footer @0x2F4: num roots = 1, roots = [0] (Scene Root). File ends at 0x2FC, matches the 764 byte size exactly, so the whole file parsed cleanly with nothing left over.

Direction check, computed only from the two translations above (light position -> target position, normalized): (0.495267, 0.274604, -0.824199). This matches the negated third column of Direct01's rotation matrix, (0.4952668, 0.2746042, -0.8241986), to 6 digits. The two independent numbers in the file agree with each other, so the geometry is internally consistent; which one the renderer should read as "the" light direction, and the sign convention (pointing from the sun or pointing at the ground), is not settled by the file alone, that needs the code (not found, see Open questions).

## Lights during a race

The global Direct3D device pointer is `DAT_00d6e208` (an `IDirect3DDevice9*`). Proven inside `FUN_004057b0` (a screen space coloured quad draw, looks like a shadow or light-pool blob, not renamed since its exact purpose was not confirmed): at 0x405b23, `CALL dword ptr [EDX + 0xE4]` with args `(DAT_00d6e208, 0x1B, 1)`, i.e. `SetRenderState(device, D3DRS_CULLMODE=0x1B, D3DCULL_NONE=1)`. The same function also calls vtable offsets 0x104 (`SetTexture`), 0x164 (`SetFVF`), 0x14C (`DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP=5, 2, pVerts, stride=0x1C)`), 0x94 (`SetRenderTarget`), 0x9C (`SetDepthStencilSurface`), all through the same `*DAT_00d6e208`, which cross-checks the IDirect3DDevice9 vtable slot table used through this whole document (slot*4 = byte offset: SetMaterial 49=0xC4, SetLight 51=0xCC, LightEnable 53=0xD4, SetRenderState 57=0xE4, SetTexture 65=0x104, SetTextureStageState 67=0x10C, DrawPrimitive 81=0x144, DrawIndexedPrimitive 82=0x148, DrawPrimitiveUP 83=0x14C, SetFVF 89=0x164).

A program wide instruction search (`search_instructions`, mnemonic `CALL`, operand containing each offset above, any base register) found, across the whole 485k instruction binary: exactly one call at offset 0xE4 (the one above), exactly one at 0xD4, exactly one at 0x10C, and zero at 0xCC and zero at 0xC4. That is far too few for a full 3D engine's material and lighting state, which means the bulk of Gamebryo's own `SetRenderState`/`SetLight`/`SetMaterial` traffic is not done through a literal `device_vtable + constant` call at the point of use in this build. The likely explanation (not proven) is that Gamebryo's NiDX9 renderer core caches the device's method pointers once into its own tables and calls through those cached pointers, which does not leave a `+0xNN]` pattern to grep for. This is a guess, and it means the exact call sites for `SetLight` and `SetMaterial` were not found in this pass.

The one 0xD4 hit, at 0x579cd3, sits inside a tiny function renamed `light_enable_thunk` (0x579cc0): `FUN_0056e7d0(param_2); (**(code**)(*param_2 + 0xD4))(param_1);`. The vtable slot (53) matches `IDirect3DDevice9::LightEnable(DWORD Index, BOOL Enable)` exactly, and no other function in the binary calls that offset. No caller of `light_enable_thunk` was found in the call graph (it is presumably reached through a function pointer or a C++ vtable slot of its own, not a direct `CALL`), so the light index and on/off value it is fed could not be confirmed. The offset match is proven from the bytes; that this is really, at runtime, `IDirect3DDevice9::LightEnable` for the world's directional light is a guess.

The one 0x10C hit, at 0x571fa0 inside `FUN_00571f90`, goes through a different global, `DAT_02f2a148`, not `DAT_00d6e208`. It could be `SetTextureStageState` on the same device, or a same-offset method on an unrelated Gamebryo class; not resolved.

No `SetLight` call (D3DLIGHT9 population: type, direction, diffuse, specular, range, attenuation) and no `D3DRS_AMBIENT` (render state 139/0x8B) write were located. What the expected behaviour would be, by the general public Gamebryo NiDX9 renderer design of this era (guess, not verified against this binary): `NiAmbientLight`'s Ambient Color feeds `D3DRS_AMBIENT`; `NiDirectionalLight` feeds one `D3DLIGHT9` with `Type = D3DLIGHT_DIRECTIONAL`, `Diffuse`/`Specular` from the NiLight's Diffuse/Specular Color, `Direction` from the light's world transform, enabled with `LightEnable(index, TRUE)`.

Both `BODY.nif` (car) and `track.nif` (Cookie_01) carry their own embedded `NiDirectionalLight`/`NiAmbientLight`/`NiPointLight` blocks (string scan of the files themselves, not the exe). `BODY.nif` and the string table used for Light.nif's own dump both reference `__MAX_Default_Light` (exe string at 0x5a3634), the default 3ds Max scene light every Max NIF exporter bakes in unless told not to. That these embedded per mesh lights are ignored at runtime, and only `Data/Public/World/Light.nif` lights the scene, is a guess, not proven against the model loading code in this pass, but it matches the symptom this task started from (663 lights over 722 car files being a uniform per file artifact, not real content) and matches `docs/engine/RENDER_MODULE.md`'s independent measurement on a related Gamebryo 2.x client ("no directional light" from the mesh).

## Material and vertex colour

Looked for `SetRenderState` calls carrying the literal render state constants 137 (`D3DRS_LIGHTING`), 141 (`COLORVERTEX`), 145/147/148/146 (`*MATERIALSOURCE`) as pushed immediates. This produced false positives, not real hits: `FUN_004cd1b0` pushes 0x93 (147) and 0x94 (148) at 0x4cd623/0x4cd63b, and `FUN_004d11d0` pushes 0x8b/0x8c/0x8d/0x8e at various addresses, but in both functions these are struct field byte offsets passed into `FUN_00448890(addr, offset, 0)`, a generic field loader used by what looks like an item/catalogue table (large tables of unrelated hex constants sit right next to them). Neither function touches the D3D device. This negative result is worth recording so the same dead end is not re-walked.

What is proven instead comes from the binary's own Gamebryo class dump/debug strings (compiled into the exe, used by some internal `Dump()` style method, not application code): `NiVertexColorProperty` (0x5bd164), its member name `m_eLighting` (0x5bdbcc), and its two enum print strings `"%s = LIGHTING_E"` (0x5bdb60) and `"%s = LIGHTING_E_A_D"` (0x5bdb70), i.e. ordinal 0 = `LIGHTING_EMISSIVE`, ordinal 1 = `LIGHTING_EMISSIVE_AMBIENT_DIFFUSE`. Cross checked against the data: Light.nif's own `NiVertexColorProperty` has lighting mode = 1, so `LIGHTING_EMISSIVE_AMBIENT_DIFFUSE`, i.e. vertex colour (where present) combines with ambient and diffuse lighting rather than being emissive only.

`BODY.nif` and `track.nif` both also carry `NiMaterialProperty`, `NiSpecularProperty`, `NiAlphaProperty`, `NiTexturingProperty` (string scan of the files). The actual ambient/diffuse/specular/emissive/alpha floats of these property blocks were not parsed in this pass (time budget), and no `SetRenderState(D3DRS_LIGHTING/COLORVERTEX/*MATERIALSOURCE, ...)` or `SetMaterial` call site was located in the exe. Open question, see below.

## Texture stages

Not resolved. `D3DTSS_COLOROP`/`COLORARG1`/`COLORARG2`/`ALPHAOP` call sites for stage 0 were not found; the only `SetTextureStageState` slot vtable hit (0x571fa0, offset 0x10C) goes through the unconfirmed `DAT_02f2a148` global discussed above, inside `FUN_00571f90`, which itself is gated by `DAT_02f2a148`, `DAT_02f29fd0`, and two fields of its `param_1` before calling `param_1`'s own vtable+0x48, a shape consistent with a per property "apply once if dirty" method, but its class identity was not confirmed.

## Fog

`world_track_init`, right after the world light file, track pieces, and gimmick loads succeed, calls (in order, 0x487xxx range): `FUN_00445200()`, `FUN_004451c0()`, `FUN_00443b70()`, `FUN_0043d9e0(0x801ec1ec)`, then reads a time of day float through `FUN_00486bf0()` and stores it at `this+0x1b4`, then `FUN_0043e520(0x3dcccccd, fVar3)`.

`FUN_0043d9e0` (0x43d9e0) is a one line setter, `*(param_1+0x9c) = param_2`; called here with the raw bit pattern 0x801ec1ec, which does not decode to a sane float (`-2.8e-39`) or an obviously meaningful int, so it reads as an opaque tag/ID, not a colour or distance. Guess: not fog specific.

`FUN_0043e520` (0x43e520) stores its float arg into `this+0x10`, stores the constant 0x3a83126f (= 0.001) into `this+0xc`, then calls a virtual method on an inner object with args `(field@8, 0.001, param3, field@0x14)`. Shape is consistent with a density/blend setter fed by the time of day value computed just before it, but nothing here names it as fog. Guess.

Later in the same function, once the sky texture is resolved, `FUN_0058b370(fVar4 > fVar3, fVar6, fVar7, uVar8)` is called with, in the common branch, `fVar6 = 30.0`, `fVar7 = 500.0`, plausible near/far or fog start/end numbers. But `FUN_0058b370` (0x58b370) disassembles to exactly one instruction: `RET 0x10`. It pops its 16 bytes of arguments and returns, doing nothing. Proven: whatever this call site once did (by argument shape, it looks like a near/far or fog range setter) is disabled in this build.

Light.nif carries no fog data (lights only). `track.nif` (Cookie_01) was string scanned and does not contain `NiFogProperty`, though the class exists in this exe's own class table (`"NiFogProperty"` string at 0x5bd434), so the engine supports it, this track's file (at least by a string level check, not a full block parse) does not use it.

`docs/packets/PACKET_REGISTRY.md` (S2C 0xC3, `FUN_0047f990`/`sub_47F990`) has 3 leading unknown int32s, a 14 int32 data block, and 2 strings; none of its fields are identified as fog colour or start/end, and this pass did not trace any of the 14 unknown ints into renderer code. Whether fog comes from this packet is unconfirmed.

Net: fog mode, colour, start and end were not found, in code or in the two data files sampled. The one candidate call site is a no-op in this build. Guess/open question throughout this section.

## Car colours and shaders

The shipped data (`Data/Public/Car/Body/High/Basic_1/`) has `BODY.nif`, `WHEEL1.nif`..`WHEEL4.nif`, and a `BODYCOLOR/` folder with 9 subfolders: `BLACK BLUE GREEN ORANGE PINK PURPPLE RED SILVER YELLOW` (`PURPPLE` is a real typo in the shipped tree, not introduced here).

Searching the exe's string table for `BODYCOLOR`, `Car/Body`, and `COLOUR` gave zero hits (ascii and unicode both tried for `BODYCOLOR`). What the exe does have is a family of `Car/FactoryCar/<PART>/%s/<PART>` format strings at 0x5a1f0c-0x5a2090: `WING`, `BUMPER`, `R_FENDER%02d`, `F_FENDER%02d`, `TIRES/%s/WHEEL%d`, `BOOSTER`, `COVER`, `CHASSIS/body`, plus `Texture/%s_Legend`, `_Epic`, `_Unique`, `_Basic`. This is a body kit / rarity tuning system, structurally different from the plain `BODY.nif` + `BODYCOLOR/<name>/` pair the shipped basic kart data uses. No code path selecting the plain `BODYCOLOR/<name>` folder, or the kart record field driving it, was found in this pass. Guess: the basic kart colour path may be built by concatenating separate literals rather than one `sprintf` format containing the literal word `BODYCOLOR`, which a string search would miss; not confirmed.

Wheels: the only wheel path string found is `Car/FactoryCar/TIRES/%s/WHEEL%d` (0x5a1fa4), which does not match the plain `Data/Public/Car/Body/High/Basic_1/WHEEL1.nif` files that exist on disk. Which code path actually loads those 4 files was not found. Open question.

`Data/Shaders/` is the stock Gamebryo/NIF Tools shader library shipped with the SDK of this era: `AlphaTextureBlender`, `BaseBumpWithSpatialGlow`, `Colorize`, `DolphinTween`, `Dot3BumpMap`, `Glass`, `HLSLSkinning`, `IndexedPaletteSkinning[_Textured]`, `LuminanceTransfer`, `MatrixPaletteSkinning[_Textured]`, `OilyFilm[WithGlow]`, `Outlining`, `ParallaxMapping`, `PerPixelLighting.fx`, `SeaFloor`, `Skin2DirLightsWarpEffect`, each as an `.NSB`/`.NSF` pair (compiled Gamebryo shader bytecode plus fragment source), a `DX8`/`DX9` subfolder pair, and 2 standalone `.fx` files (`Chrome.fx`, `FXSkinning.fx`). Neither `BODY.nif` nor `track.nif` (string scanned) names any of these files, or carries an `NiShaderProperty`/material effect block; both use only the fixed function `NiMaterialProperty`/`NiTexturingProperty`/`NiAlphaProperty`/`NiSpecularProperty` set. Only these 2 files were sampled, not the whole tree, but directionally: for the car body and this track, the programmable shader library sits on disk unused, the fixed function pipeline is what has to be matched.

## What the renderer must do

1. Load `Data/Public/World/Light.nif` once per world/track init and take it as the sole active light set for that scene. Proven: this is what `world_track_init` does (0x4875c0 -> `light_file_load` at 0x443c10).
2. Read the `NiAmbientLight` block's Ambient Color as the scene ambient term (RGB 200,200,200 / 0.784314 in the sample file), not its Diffuse/Specular, which are zero. Proven from file bytes.
3. Read the `NiDirectionalLight` block's Diffuse (and Specular, identical in the sample, RGB 255,246,235) as the sun colour; its own Ambient is zero, it contributes no separate ambient term. Proven from file bytes.
4. Derive the sun direction either from the `Direct01` node's world rotation or from `normalize(Direct01.Target.translation - Direct01.translation)`; both agree in the sample file ((0.4953, 0.2746, -0.8242) up to sign). Pick and document one sign convention; not settled by the data.
5. Ignore `NiDirectionalLight`/`NiPointLight`/`NiSpotLight` blocks embedded in prop, car, and track NIFs; treat them as exporter artifacts (`__MAX_Default_Light`). Guess, but consistent with the dark render symptom and with RENDER_MODULE.md's independent measurement.
6. Do not add fog for tracks like Cookie_01 until a real fog data source is found: the one candidate fog related call in `world_track_init` is a no-op stub (`FUN_0058b370` = `RET 0x10`) in this build, and the sampled track NIF carries no `NiFogProperty`. Guess/open item.
7. Where a mesh carries `NiVertexColorProperty` with `LightingMode = LIGHTING_EMISSIVE_AMBIENT_DIFFUSE` (proven for the world light rig's own property, ordinal 1), combine emissive, ambient, and diffuse with vertex colour, matching RENDER_MODULE.md's existing emissive+ambient model plus an explicit vertex colour term.
8. Keep car body colour selection on the existing `Data/Public/Car/Body/High/<car>/BODYCOLOR/<COLOUR>/` folder convention (confirmed to exist on disk for the basic karts); do not switch to the exe's `Car/FactoryCar/Texture/*_Legend|_Epic|_Unique|_Basic` strings for basic karts, that looks like a separate, unconfirmed tuning subsystem.
9. Do not assume any `Data/Shaders/*.NSB/.NSF` file is required to match the car or this track's look; the sampled NIFs use only the fixed function property set.

## Constants read

| Address | Bytes / value | What |
|---|---|---|
| 0x5a6208 | ascii | `"./Data/Public/World/light"` |
| 0x443c10 |, | `light_file_load`, appends `%s.nif`, calls virtual Load |
| 0x4875c0 |, | `world_track_init` |
| 0x4876fa |, | call site, builds and loads the world light file |
| Light.nif @0x27 | `00 00 02 0a` | version u32 = 0x0A020000 (10.2.0.0) |
| Light.nif @0x2B | `00 00 00 00` | user version u32 = 0 |
| Light.nif @0x2F | `07 00 00 00` | num blocks u32 = 7 |
| Light.nif @0x33 | `05 00` | num block types u16 = 5 |
| Light.nif @0x13F | `00 00 00 00` | NiVertexColorProperty vertex mode u32 = 0 |
| Light.nif @0x143 | `01 00 00 00` | NiVertexColorProperty lighting mode u32 = 1 |
| Light.nif @0x15D | 3 floats | Direct01 translation = (-12336.216796875, -1002.2261962890625, 20311.640625) |
| Light.nif @0x169 | 9 floats | Direct01 world rotation, see Light file section |
| Light.nif @0x1F4 | `00 00 80 3f` | NiDirectionalLight dimmer = 1.0 |
| Light.nif @0x1F8 | 3 floats = 0 | NiDirectionalLight ambient = (0,0,0) |
| Light.nif @0x204 | 3 floats | NiDirectionalLight diffuse = (1.0, 0.964706, 0.921569) |
| Light.nif @0x210 | 3 floats | NiDirectionalLight specular = same as diffuse |
| Light.nif @0x239 | 3 floats | Direct01.Target translation = (0.132080078125, 5837.75, -217.9013671875) |
| Light.nif @0x2CC | `00 00 80 3f` | NiAmbientLight dimmer = 1.0 |
| Light.nif @0x2D0 | 3 floats | NiAmbientLight ambient = (0.784314, 0.784314, 0.784314) |
| Light.nif @0x2DC, 0x2E8 | 3 floats = 0 each | NiAmbientLight diffuse, specular = (0,0,0) |
| 0x00d6e208 | pointer | `DAT_00d6e208`, global `IDirect3DDevice9*` |
| 0x405b23 | `ff92e4000000` | `CALL [EDX+0xE4]` = `SetRenderState(device, 0x1B, 1)` |
| 0x579cd3 | `ff90d4000000` | `CALL [EAX+0xD4]`, vtable slot of `LightEnable`, inside `light_enable_thunk` (0x579cc0) |
| 0x571fa0 | `ff900c010000` | `CALL [EAX+0x10C]` via `DAT_02f2a148`, inside `FUN_00571f90`, identity unconfirmed |
| 0x43e520 | `0x3dcccccd`, `0x3a83126f` | floats 0.1 and 0.001, stored by `FUN_0043e520` |
| 0x58b370 | `c2 10 00` | `RET 0x10`, no-op stub |
| 0x43d9e0 | `0x801ec1ec` | opaque constant stored by a 1 line setter |
| 0x5a1f0c-0x5a2090 | ascii | `Car/FactoryCar/*` format strings (WING, BUMPER, FENDER, TIRES, BOOSTER, COVER, CHASSIS, Texture rarity suffixes) |
| 0x5a3634 | ascii | `"__MAX_Default_Light"` |
| 0x5bd164 | ascii | `"NiVertexColorProperty"` |
| 0x5bdbcc | ascii | `"m_eLighting"` |
| 0x5bdb60, 0x5bdb70 | ascii | `"%s = LIGHTING_E"`, `"%s = LIGHTING_E_A_D"` |
| 0x5bd434 | ascii | `"NiFogProperty"` |

## Open questions

- No `SetLight` call site found: the exact `D3DLIGHT9` population (type, direction sign, range, attenuation) from `NiDirectionalLight`/`NiAmbientLight` data is unproven.
- No `SetMaterial` call site found, and no `SetRenderState` call site found for `D3DRS_LIGHTING`(137), `COLORVERTEX`(141), `DIFFUSE`/`AMBIENT`/`EMISSIVE`/`SPECULARMATERIALSOURCE`(145/147/148/146). Two naive constant push hits turned out to be unrelated catalogue field offsets (`FUN_004cd1b0`, `FUN_004d11d0`).
- `D3DTSS_COLOROP`/`COLORARG1`/`COLORARG2`/`ALPHAOP` for texture stage 0: not found.
- Whether fog is really disabled everywhere, or only the one call site in `world_track_init` happens to be a stub while another exists elsewhere: unresolved.
- Whether the 0xC3 `ROOM_DATA` packet's 14 unknown int32s carry fog fields: unresolved, not traced.
- Which kart record field selects the `BODYCOLOR/<name>` folder, and how (or whether) the `FactoryCar/Texture/*_Legend|_Epic|_Unique|_Basic` strings interact with it: unresolved.
- Whether `track.nif`'s lack of `NiFogProperty` generalizes to other tracks: only Cookie_01 was sampled.
- `light_enable_thunk` (0x579cc0) has no located caller in the static call graph; how it is reached (vtable slot of some class, most likely) and what values it is called with were not found.
