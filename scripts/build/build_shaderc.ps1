# builds bgfx shaderc once in build-bgfx-tools kept out of the engine build so glslang stays out too
$root  = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$src   = Join-Path $root 'thirdparty\bgfx'
$build = Join-Path $root 'build-bgfx-tools'
if (-not (Test-Path (Join-Path $src 'bgfx\include\bgfx\bgfx.h'))) {
    throw "bgfx submodules are missing, run: git submodule update --init --recursive"
}
cmake -S $src -B $build -G 'Visual Studio 17 2022' -A x64 `
    -DBGFX_BUILD_TOOLS=ON -DBGFX_BUILD_TOOLS_SHADER=ON `
    -DBGFX_BUILD_TOOLS_BIN2C=OFF -DBGFX_BUILD_TOOLS_GEOMETRY=OFF -DBGFX_BUILD_TOOLS_TEXTURE=OFF `
    -DBGFX_BUILD_EXAMPLES=OFF -DBGFX_INSTALL=OFF -DBGFX_CUSTOM_TARGETS=OFF
if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }
cmake --build $build --config Release --target shaderc
if ($LASTEXITCODE -ne 0) { throw "shaderc build failed" }
Write-Host "shaderc: $build\cmake\bgfx\Release\shaderc.exe"
