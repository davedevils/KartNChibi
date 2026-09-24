# Third party

What is in this folder and how it comes in.

| Folder | What | How |
|---|---|---|
| `bgfx/` | bgfx.cmake with bgfx, bimg and bx, the renderer of the client rewrite | git submodules, pinned, `git submodule update --init --recursive` |
| `glad/` | generated OpenGL loader | source, checked in |
| `glfw/` | Windows, Linux and macOS windows and input | source, built static |
| `stb/` | stb_image and stb_truetype | headers |
| `tinyddsloader/` | DDS decode fallback for the old texture path | header |
| `tinygltf/` `tinyobj/` `ufbx/` | glTF, OBJ and FBX loaders of the old model path | headers |
| `tinyfiledialogs/` | native file dialogs for the tools | one C file |
| `lua54/` | Lua 5.4 scripting | source |
| `recast/` | Recast and Detour navigation | source |
| `miniaudio/` | audio playback | header |
| `libjpeg/` | JPEG decode fallback, off by default | source |
| `mariadb-connector-c/` | MariaDB client for the Windows server build | source and prebuilt lib, see DOWNLOAD_LIBS.md |

Everything is MIT, BSD, zlib, public domain or the IJG licence, except
`mariadb-connector-c/` which is LGPL 2.1, the only copyleft component here.

A real licence file sits in only 3 folders, `bgfx/` (and its nested `bgfx/bgfx/`,
`bgfx/bimg/`, `bgfx/bx/`), `glfw/`, `tinyddsloader/`, plus `mariadb-connector-c/`
now carries `COPYING.LIB`. The rest carry their licence text inline in the source
header, `lua54/`, `miniaudio/`, `stb/`, `tinygltf/`, `tinyobj/`, `tinyfiledialogs/`,
`libjpeg/` (its own `LICENSE_IJG.txt`). `glad/` and `recast/` carry no licence text
in this tree at all, see their upstream repository. The full list with upstream
URLs sits in [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md) at the repo root.
