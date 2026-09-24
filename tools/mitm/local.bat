@echo off
title Local KnC server
if exist "%~dp0mitm.local.bat" call "%~dp0mitm.local.bat"
if not defined KNC_DEVCLIENT_DIR goto :needenv

echo === point the launcher and client back at our own login 50017 ===
(
echo IP=127.0.0.1
echo Port=50017
echo gethostbynameForcedAdapterIP=127.0.0.1
echo debug=1
)> "%KNC_DEVCLIENT_DIR%\network.ini"
(
echo ServerIP=127.0.0.1
echo ServerPort=50017
echo GamePath=KnC.exe
echo SavedUser=test
)> "%KNC_DEVCLIENT_DIR%\launcher.ini"

echo === stop the proxy if it is running ===
taskkill /IM mitmprivategg.exe /F >nul 2>&1

echo === launch through the launcher on our own login ===
start "" /D "%KNC_DEVCLIENT_DIR%" "%KNC_DEVCLIENT_DIR%\KnCLauncher.exe"
echo.
echo client is on our own local server
goto :eof

:needenv
echo set KNC_DEVCLIENT_DIR first, see mitm.env.example.bat
exit /b 1
