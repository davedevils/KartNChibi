@echo off
echo === Building EngineDLL Proxy ===

cd /d "%~dp0"

:: Find Visual Studio
set "VCVARS="
if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars32.bat" (
    set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars32.bat"
)
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" (
    set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat"
)
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars32.bat" (
    set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars32.bat"
)

if "%VCVARS%"=="" (
    echo ERROR: Visual Studio not found
    exit /b 1
)

echo Using: %VCVARS%
call "%VCVARS%"

:: Compile proxy DLL
echo.
echo Compiling proxy DLL...
cl /nologo /LD /O2 /Fe:EngineDLL.dll EngineDLL_proxy.cpp /link /DEF:EngineDLL_proxy.def

if errorlevel 1 (
    echo BUILD FAILED
    exit /b 1
)

echo.
echo === Build Successful ===
echo.
echo To install:
echo 1. Copy EngineDLL.dll to DevClient folder (rename original to EngineDLL_orig.dll first!)
echo 2. Run KnC.exe
echo 3. Check EngineDLL_proxy.log for intercepted calls
echo.

