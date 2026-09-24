@echo off
title KnC Server Build
color 0E

echo ===============================================
echo           KnC Server Build (CMake)
echo ===============================================
echo.

:: find CMake
set CMAKE="C:\Program Files\CMake\bin\cmake.exe"
if not exist %CMAKE% (
    echo [ERROR] CMake not found! Install CMake.
    pause
    exit /b 1
)

:: create build dir
if not exist build mkdir build
cd build

:: run cmake
echo [1/3] Configuring with CMake...
%CMAKE% .. -G "Visual Studio 17 2022" -A x64
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake configuration failed!
    cd ..
    pause
    exit /b 1
)

:: build
echo.
echo [2/3] Building Release...
%CMAKE% --build . --config Release
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build failed!
    cd ..
    pause
    exit /b 1
)

:: copy to dist
echo.
echo [3/3] Copying to dist...
cd ..
if not exist dist mkdir dist
if not exist dist\config mkdir dist\config
if not exist dist\logs mkdir dist\logs

copy /Y build\bin\Release\LoginServer.exe dist\
copy /Y build\bin\Release\GameServer.exe dist\

:: copy config if not exists
if not exist dist\config\server.ini (
    copy /Y dist\config\server.ini dist\config\server.ini 2>nul
)

echo.
echo ===============================================
echo   Build successful!
echo   Output: dist\LoginServer.exe
echo          dist\GameServer.exe
echo.
echo   Run dist\start.bat to launch servers
echo ===============================================

pause
