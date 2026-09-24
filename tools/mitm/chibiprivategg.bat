@echo off
title Chibikart via MITM
if exist "%~dp0mitm.local.bat" call "%~dp0mitm.local.bat"
if not defined KNC_DEVCLIENT_DIR goto :needenv
if not defined KNC_MITM_DIR goto :needenv

echo === point the launcher and client at the proxy port 50617 ===
(
echo IP=127.0.0.1
echo Port=50617
echo gethostbynameForcedAdapterIP=127.0.0.1
echo debug=1
)> "%KNC_DEVCLIENT_DIR%\network.ini"
(
echo ServerIP=127.0.0.1
echo ServerPort=50617
echo GamePath=KnC.exe
echo SavedUser=test
)> "%KNC_DEVCLIENT_DIR%\launcher.ini"

echo === restart the proxy 50617 to chibikart ===
taskkill /IM mitmprivategg.exe /F >nul 2>&1
start "MITM chibikart" "%KNC_MITM_DIR%\mitmprivategg.exe" "%KNC_MITM_DIR%\mitm.local.ini"
timeout /t 1 >nul

echo === launch the launcher, the proxy fakes its token then the game enters chibikart ===
start "" /D "%KNC_DEVCLIENT_DIR%" "%KNC_DEVCLIENT_DIR%\KnCLauncher.exe"
echo.
echo type any user pass in the launcher, the proxy accepts it and injects the configured account
echo watch the wire live   %KNC_MITM_DIR%\mitm_packets.log
goto :eof

:needenv
echo set KNC_DEVCLIENT_DIR and KNC_MITM_DIR first, see mitm.env.example.bat
exit /b 1
