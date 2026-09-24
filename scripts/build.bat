@echo off
REM lowercase config name so output directories match

set CONFIG=release
if /i "%~1"=="debug" set CONFIG=debug
if /i "%~1"=="release" set CONFIG=release

echo Building %CONFIG%...

if not exist build mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A Win32
cmake --build . --config %CONFIG% -- /m
cd ..

echo.
echo Executables in: %CONFIG%\
dir %CONFIG%\*.exe 2>nul
