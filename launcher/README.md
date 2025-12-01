# KnC Launcher

Modern launcher for Kart n' Crazy with WebView2.

## Features

- HTML/CSS ui
- Login with secure token auth
- Remembers username
- Auto-launches game

## Build

```bash
cmake -B build
cmake --build build --config Release
```

Output: `build/Release/KnCLauncher.exe`

## Requirements

- Windows 10/11
- WebView2 Runtime (usualy pre-installed)

## Usage

1. Put `KnCLauncher.exe` next to `KnC.exe`
2. Run launcher
3. Login and click PLAY

## Config

`launcher.ini`:
```ini
ServerIP=127.0.0.1
ServerPort=50017
GamePath=KnC.exe
SavedUser=
RegisterURL=https://example.com/register
FindAccountURL=https://example.com/forgot-password
```

## Protocol

Login uses CMD `0xFE`:

**Request:** `[size:2][0xFE][flag:1][reserved:4][user\0][pass\0]`  
**Response:** `[size:2][0xFE][flag:1][reserved:4][success:1][token\0][msg\0]`

Token is 32-char secure random string generated server-side.
