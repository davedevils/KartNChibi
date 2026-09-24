@echo off
REM reconfigures cmake and rebuilds release only

echo ========================================
echo KnC Engine - Quick Rebuild (Release)
echo ========================================
echo.

set "ROOT_DIR=%~dp0.."
cd /d "%ROOT_DIR%"

echo [1/3] Cleaning old build files...
if exist "build" (
    echo Removing build directory...
    rmdir /s /q "build"
)

echo.
echo [2/3] Reconfiguring CMake...
cmake -B build -S . -G "Visual Studio 17 2022" -A x64
if errorlevel 1 (
    echo ERROR: CMake configuration failed!
    pause
    exit /b 1
)

echo.
echo [3/3] Building Release...
cmake --build build --config Release -- /m /v:minimal
if errorlevel 1 (
    echo ERROR: Release build failed!
    pause
    exit /b 1
)

echo.
echo ========================================
echo Build completed successfully!
echo ========================================
echo.
echo Executables are in: Release\
echo.
pause

