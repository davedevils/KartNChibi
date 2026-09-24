# Third Party Notices

Every vendored library in this repository, one row per folder under `thirdparty/`
and per vendored library under `server/lib`. `thirdparty/README.md` gives the short
version, this file is the full list with the upstream source and where the licence
text sits. `LICENSE.md` section 10 is a short summary, this file is the source of
truth.

## thirdparty/

| Path | Library | Licence | Upstream | Licence text |
|---|---|---|---|---|
| `thirdparty/bgfx/` | bgfx.cmake, the build wrapper | CC0 | https://github.com/bkaradzic/bgfx.cmake | `thirdparty/bgfx/LICENSE` |
| `thirdparty/bgfx/bgfx/` | bgfx (git submodule) | BSD 2-Clause | https://github.com/bkaradzic/bgfx | `thirdparty/bgfx/bgfx/LICENSE` |
| `thirdparty/bgfx/bimg/` | bimg (git submodule) | BSD 2-Clause | https://github.com/bkaradzic/bimg | `thirdparty/bgfx/bimg/LICENSE` |
| `thirdparty/bgfx/bx/` | bx (git submodule) | BSD 2-Clause | https://github.com/bkaradzic/bx | `thirdparty/bgfx/bx/LICENSE` |
| `thirdparty/glad/` | glad, generated OpenGL loader | Public Domain or MIT | https://glad.dav1d.de | no file, see `thirdparty/glad/README.md` |
| `thirdparty/glfw/` | GLFW | Zlib | https://www.glfw.org | `thirdparty/glfw/LICENSE.md` |
| `thirdparty/libjpeg/` | libjpeg, the Independent JPEG Group software | IJG | https://www.ijg.org | `thirdparty/libjpeg/LICENSE_IJG.txt` |
| `thirdparty/lua54/` | Lua 5.4 | MIT | https://www.lua.org | inline at the end of `lua.h`, no separate file |
| `thirdparty/mariadb-connector-c/` | MariaDB Connector/C | LGPL 2.1 | https://mariadb.com/downloads/connectors/connectors-data-access/c-connector | `thirdparty/mariadb-connector-c/COPYING.LIB` |
| `thirdparty/miniaudio/` | miniaudio | Public Domain or MIT-0, dual | https://github.com/mackron/miniaudio | inline at the end of `miniaudio.h` |
| `thirdparty/recast/` | Recast and Detour | Zlib | https://github.com/recastnavigation/recastnavigation | no file and no inline text in this vendored copy, see the upstream repository |
| `thirdparty/stb/` | stb_image, stb_truetype and friends | Public Domain or MIT, dual | https://github.com/nothings/stb | inline at the end of each header |
| `thirdparty/tinyddsloader/` | tinyddsloader | MIT | https://github.com/benikabocha/tinyddsloader | `thirdparty/tinyddsloader/LICENSE` |
| `thirdparty/tinyfiledialogs/` | tinyfiledialogs | Zlib | https://sourceforge.net/projects/tinyfiledialogs | inline in the header comment of `tinyfiledialogs.h` |
| `thirdparty/tinygltf/` | tinygltf | MIT | https://github.com/syoyo/tinygltf | inline at the top of `tiny_gltf.h` |
| `thirdparty/tinyobj/` | tinyobjloader | MIT | https://github.com/tinyobjloader/tinyobjloader | inline at the top of `tiny_obj_loader.h`, the stub `tinyobj_loader.h` carries its own short notice |
| `thirdparty/ufbx/` | ufbx | MIT | https://github.com/ufbx/ufbx | no file and no inline text in this vendored copy, see the upstream repository |

## server/lib

| Path | Library | Licence | Upstream | Licence text |
|---|---|---|---|---|
| `server/lib/asio/` | standalone Asio | Boost Software Licence 1.0 | https://think-async.com/Asio | inline at the top of every header, no separate file |
| `server/lib/httplib/` | cpp-httplib | MIT | https://github.com/yhirose/cpp-httplib | inline at the top of `httplib.h` |
| `server/lib/json/` | nlohmann json | MIT | https://github.com/nlohmann/json | inline SPDX tags through `json.hpp`, no separate file |
| `server/lib/libmariadb.lib` | MariaDB Connector/C, same binary as thirdparty | LGPL 2.1 | https://mariadb.com/downloads/connectors/connectors-data-access/c-connector | `thirdparty/mariadb-connector-c/COPYING.LIB` |

`server/lib/crypto/` is project code, not a vendored library, it calls the
platform bcrypt or openssl API and carries no third party notice.
