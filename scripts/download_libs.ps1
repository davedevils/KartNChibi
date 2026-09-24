
Write-Host "Downloading third-party libraries..." -ForegroundColor Cyan

$dirs = @(
    "thirdparty/miniaudio",
    "thirdparty/tinyobj",
    "thirdparty/tinygltf",
    "thirdparty/ufbx"
)

foreach ($dir in $dirs) {
    if (-not (Test-Path $dir)) {
        New-Item -ItemType Directory -Force -Path $dir | Out-Null
        Write-Host "Created directory: $dir" -ForegroundColor Green
    }
}

$downloads = @(
    @{
        Url = "https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h"
        Output = "thirdparty/miniaudio/miniaudio.h"
        Name = "miniaudio (Public Domain)"
    },
    @{
        Url = "https://raw.githubusercontent.com/nothings/stb/master/stb_truetype.h"
        Output = "thirdparty/stb/stb_truetype.h"
        Name = "stb_truetype (Public Domain)"
    },
    @{
        Url = "https://raw.githubusercontent.com/tinyobjloader/tinyobjloader/release/tiny_obj_loader.h"
        Output = "thirdparty/tinyobj/tiny_obj_loader.h"
        Name = "tinyobjloader (MIT)"
    },
    @{
        Url = "https://raw.githubusercontent.com/syoyo/tinygltf/release/tiny_gltf.h"
        Output = "thirdparty/tinygltf/tiny_gltf.h"
        Name = "tinygltf (MIT)"
    },
    @{
        Url = "https://raw.githubusercontent.com/ufbx/ufbx/master/ufbx.h"
        Output = "thirdparty/ufbx/ufbx.h"
        Name = "ufbx header (MIT)"
    },
    @{
        Url = "https://raw.githubusercontent.com/ufbx/ufbx/master/ufbx.c"
        Output = "thirdparty/ufbx/ufbx.c"
        Name = "ufbx implementation (MIT)"
    }
)

$success = 0
$failed = 0

foreach ($download in $downloads) {
    try {
        Write-Host "Downloading $($download.Name)..." -ForegroundColor Yellow
        Invoke-WebRequest -Uri $download.Url -OutFile $download.Output -ErrorAction Stop
        Write-Host "  OK Downloaded to $($download.Output)" -ForegroundColor Green
        $success++
    }
    catch {
        Write-Host "  ERROR Failed to download $($download.Name): $_" -ForegroundColor Red
        $failed++
    }
}

Write-Host ""
Write-Host "Download complete!" -ForegroundColor Cyan
Write-Host "  Success: $success" -ForegroundColor Green
if ($failed -eq 0) {
    Write-Host "  Failed: $failed" -ForegroundColor Green
} else {
    Write-Host "  Failed: $failed" -ForegroundColor Red
}

if ($failed -eq 0) {
    Write-Host ""
    Write-Host "All libraries downloaded successfully!" -ForegroundColor Green
    Write-Host "You can now build the engine with full functionality." -ForegroundColor Cyan
} else {
    Write-Host ""
    Write-Host "Some downloads failed. Please check your internet connection." -ForegroundColor Yellow
    Write-Host "You can manually download missing libraries from:" -ForegroundColor Yellow
    Write-Host "  https://github.com/mackron/miniaudio" -ForegroundColor White
    Write-Host "  https://github.com/nothings/stb" -ForegroundColor White
    Write-Host "  https://github.com/tinyobjloader/tinyobjloader" -ForegroundColor White
    Write-Host "  https://github.com/syoyo/tinygltf" -ForegroundColor White
    Write-Host "  https://github.com/ufbx/ufbx" -ForegroundColor White
}
