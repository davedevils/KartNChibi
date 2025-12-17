@echo off
setlocal enabledelayedexpansion
call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars32.bat"

set ENGINE=..\..\engine
set COMMON=..\..\common
set RAYLIB=..\..\external\raylib
set OBJDIR=build\intermediate
set OUTDIR=build\release

REM Reuse NifEngine from map_viewer (already compiled)
set NIFOBJ=..\map_viewer\build\intermediate

echo ============================================
echo KnC DAT Manager - Build
echo ============================================

if not exist %OBJDIR% mkdir %OBJDIR%
if not exist %OUTDIR% mkdir %OUTDIR%

set THIRDPARTY=..\..\thirdparty
set LIBJPEG=%THIRDPARTY%\libjpeg
set CFLAGS=/nologo /EHsc /MD /O2 /GL /std:c++17 /DNIFLIB_STATIC_LINK /DNDEBUG /DKNC_USE_LIBJPEG_FALLBACK /c
set INCLUDES=/I%ENGINE%\niflib\include /I%ENGINE%\niflib /I%ENGINE%\bridge /I%ENGINE%\texture /I%THIRDPARTY% /I%LIBJPEG% /I%COMMON% /I%RAYLIB%\include
set LINKFLAGS=/nologo /LTCG /OPT:REF /OPT:ICF
set LIBS=raylib.lib opengl32.lib gdi32.lib winmm.lib kernel32.lib user32.lib shell32.lib

echo.
echo Compiling DatManager...
cl %CFLAGS% %INCLUDES% dat_manager.cpp /Fo:%OBJDIR%\dat_manager.obj
if errorlevel 1 goto error

echo Copying NifEngine objects...
REM Copy all niflib objects except mapviewer
for %%f in (%NIFOBJ%\*.obj) do (
    if /I not "%%~nxf"=="mapviewer.obj" (
        copy /Y "%%f" "%OBJDIR%\" >nul 2>&1
    )
)

echo Linking...
link %LINKFLAGS% /OUT:%OUTDIR%\KnC_DatManager.exe %OBJDIR%\*.obj /LIBPATH:%RAYLIB%\lib %LIBS%
if errorlevel 1 goto error

echo.
echo ============================================
echo BUILD OK: %OUTDIR%\KnC_DatManager.exe
echo ============================================
goto end

:error
echo BUILD FAILED!
exit /b 1

:end
