@echo off
echo ========================================
echo KnC Server - MariaDB DLL Setup
echo ========================================
echo.

REM Check common installation paths
set MARIADB_PATH=

if exist "C:\Program Files\MariaDB\MariaDB Connector C 64-bit\lib\libmariadb.dll" (
    set MARIADB_PATH=C:\Program Files\MariaDB\MariaDB Connector C 64-bit\lib
    goto :found
)

if exist "C:\Program Files\MariaDB\MariaDB Connector C\lib\libmariadb.dll" (
    set MARIADB_PATH=C:\Program Files\MariaDB\MariaDB Connector C\lib
    goto :found
)

if exist "C:\MariaDB\lib\libmariadb.dll" (
    set MARIADB_PATH=C:\MariaDB\lib
    goto :found
)

echo ERROR: libmariadb.dll not found!
echo.
echo Please install MariaDB Connector/C:
echo 1. Go to: https://mariadb.com/downloads/connectors/
echo 2. Download "MariaDB Connector/C" for Windows 64-bit (MSI)
echo 3. Install it
echo 4. Run this script again
echo.
pause
exit /b 1

:found
echo Found MariaDB at: %MARIADB_PATH%
echo.

REM Copy DLL to server directories
set SERVER_DIR=%~dp0..\build\bin\Release

echo Copying libmariadb.dll to %SERVER_DIR%...
copy "%MARIADB_PATH%\libmariadb.dll" "%SERVER_DIR%\" /Y

if exist "%SERVER_DIR%\libmariadb.dll" (
    echo.
    echo SUCCESS! libmariadb.dll copied successfully.
    echo You can now run LoginServer.exe and GameServer.exe
) else (
    echo.
    echo ERROR: Failed to copy DLL
)

echo.
pause

