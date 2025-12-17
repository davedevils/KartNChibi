@echo off
setlocal enabledelayedexpansion
call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars32.bat"

set ENGINE=..\..\engine
set COMMON=..\..\common
set RAYLIB=..\..\external\raylib
set OBJDIR=build\intermediate
set OUTDIR=build\release

echo ============================================
echo KnC Race Prototype - Build
echo ============================================

if not exist %OBJDIR% mkdir %OBJDIR%
if not exist %OUTDIR% mkdir %OUTDIR%

set THIRDPARTY=..\..\thirdparty
set LIBJPEG=%THIRDPARTY%\libjpeg
set CFLAGS=/nologo /EHsc /MD /O2 /GL /std:c++17 /DNIFLIB_STATIC_LINK /DNDEBUG /DKNC_USE_LIBJPEG_FALLBACK /c
set INCLUDES=/I%ENGINE%\niflib\include /I%ENGINE%\niflib /I%ENGINE%\bridge /I%ENGINE%\texture /I%THIRDPARTY% /I%LIBJPEG% /I%COMMON% /I%RAYLIB%\include
set LINKFLAGS=/nologo /LTCG /OPT:REF /OPT:ICF
set LIBS=raylib.lib opengl32.lib gdi32.lib winmm.lib kernel32.lib user32.lib shell32.lib

echo Compiling...
cl %CFLAGS% %INCLUDES% race_prototype.cpp /Fo:%OBJDIR%\race_prototype.obj
if errorlevel 1 goto error

echo Linking...
link %LINKFLAGS% /OUT:%OUTDIR%\KnC_RacePrototype.exe %OBJDIR%\race_prototype.obj /LIBPATH:%RAYLIB%\lib %LIBS%
if errorlevel 1 goto error

echo.
echo BUILD OK: %OUTDIR%\KnC_RacePrototype.exe
%OUTDIR%\KnC_RacePrototype.exe
goto end

:error
echo BUILD FAILED!
exit /b 1

:end

