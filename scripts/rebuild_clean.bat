@echo off
REM reconfigures cmake and rebuilds everything

echo ========================================
echo KnC Engine - Clean Rebuild
echo ========================================
echo.

set "ROOT_DIR=%~dp0.."
cd /d "%ROOT_DIR%"

echo [1/4] Cleaning old build files...
if exist "build" (
    echo Removing build directory...
    rmdir /s /q "build"
)

echo.
echo [2/4] Reconfiguring CMake...
cmake -B build -S . -G "Visual Studio 17 2022" -A x64
if errorlevel 1 (
    echo ERROR: CMake configuration failed!
    pause
    exit /b 1
)

echo.
echo [3/4] Building Release...
cmake --build build --config Release -- /m /v:minimal
if errorlevel 1 (
    echo ERROR: Release build failed!
    pause
    exit /b 1
)

echo.
echo [4/4] Building Debug...
cmake --build build --config Debug -- /m /v:minimal
if errorlevel 1 (
    echo ERROR: Debug build failed!
    pause
    exit /b 1
)

echo.
echo ========================================
echo Build completed successfully!
echo ========================================
pause

