@echo off
REM Quick test build for shared library

echo === Building Shared Library Tests ===
echo.

set TEST_DIR=%~dp0
set SHARED_DIR=%TEST_DIR%..\..\shared
set OUT_DIR=%TEST_DIR%out

if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

echo Compiling test_types.cpp...
cl.exe /nologo ^
    /std:c++17 ^
    /EHsc ^
    /W4 ^
    /I"%SHARED_DIR%\include" ^
    /Fe:"%OUT_DIR%\test_types.exe" ^
    "%TEST_DIR%\test_types.cpp"

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Compilation failed!
    exit /b 1
)

echo [OK] Compilation successful
echo.

echo === Running Tests ===
echo.
"%OUT_DIR%\test_types.exe"

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Tests failed!
    exit /b 1
)

echo.
echo [SUCCESS] All tests passed!
pause

