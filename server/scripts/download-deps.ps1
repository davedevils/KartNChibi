# Download dependencies for KnC Server

$ErrorActionPreference = "Stop"

Write-Host "=== Downloading KnC Server Dependencies ===" -ForegroundColor Cyan

# Create lib directories
$dirs = @("lib/asio/include", "lib/json/include", "lib/httplib")
foreach ($dir in $dirs) {
    if (!(Test-Path $dir)) {
        New-Item -ItemType Directory -Force -Path $dir | Out-Null
    }
}

# ASIO standalone (header-only)
$asioVersion = "asio-1-30-2"
$asioUrl = "https://github.com/chriskohlhoff/asio/archive/refs/tags/$asioVersion.zip"
$asioZip = "asio.zip"

if (!(Test-Path "lib/asio/include/asio.hpp")) {
    Write-Host "Downloading ASIO..." -ForegroundColor Yellow
    Invoke-WebRequest -Uri $asioUrl -OutFile $asioZip
    Expand-Archive -Path $asioZip -DestinationPath "lib/asio/temp" -Force
    Move-Item -Force "lib/asio/temp/asio-$asioVersion/asio/include/*" "lib/asio/include/"
    Remove-Item -Recurse -Force "lib/asio/temp"
    Remove-Item $asioZip
    Write-Host "ASIO downloaded!" -ForegroundColor Green
} else {
    Write-Host "ASIO already present" -ForegroundColor Green
}

# nlohmann/json (single header)
$jsonUrl = "https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp"
$jsonDir = "lib/json/include/nlohmann"

if (!(Test-Path "$jsonDir/json.hpp")) {
    Write-Host "Downloading nlohmann/json..." -ForegroundColor Yellow
    New-Item -ItemType Directory -Force -Path $jsonDir | Out-Null
    Invoke-WebRequest -Uri $jsonUrl -OutFile "$jsonDir/json.hpp"
    Write-Host "nlohmann/json downloaded!" -ForegroundColor Green
} else {
    Write-Host "nlohmann/json already present" -ForegroundColor Green
}

# cpp-httplib (single header)
$httplibUrl = "https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h"

if (!(Test-Path "lib/httplib/httplib.h")) {
    Write-Host "Downloading cpp-httplib..." -ForegroundColor Yellow
    Invoke-WebRequest -Uri $httplibUrl -OutFile "lib/httplib/httplib.h"
    Write-Host "cpp-httplib downloaded!" -ForegroundColor Green
} else {
    Write-Host "cpp-httplib already present" -ForegroundColor Green
}

Write-Host "`n=== All dependencies ready! ===" -ForegroundColor Cyan
Write-Host "Run: cmake -B build && cmake --build build" -ForegroundColor White

