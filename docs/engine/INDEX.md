# KnC Engine Docs

## Doc Map

| Document | Scope | Key Files |
|----------|-------|-----------|
| [RENDER_MODULE.md](RENDER_MODULE.md) | The bgfx renderer and the NIF reader, the only engine stack in this repository | `engine/render/`, `engine/formats/`, `thirdparty/bgfx/` |

This repository ships only `engine/render` and `engine/formats`. The older RHI based engine (rhi, graphics, renderer, shaders, nif_import, niflib, models, and the rest of the subsystems that only the old stack used) is not part of this repository, it stayed in the private development tree until its last consumers moved to the bgfx stack.

## Namespaces

- `KnC::Render`: the bgfx scene renderer, `engine/render/`
- `KnC::`: the NIF, KF and KFM readers and the glTF writer, `engine/formats/`
