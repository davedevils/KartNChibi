@echo off
REM KnC Engine - Generate HTML Documentation
REM Output: docs\engine\html\
REM Requires: Doxygen (winget install doxygen)

cd /d "%~dp0..\.."

where doxygen >nul 2>&1
if %errorlevel% neq 0 (
    if exist "C:\Program Files\doxygen\bin\doxygen.exe" (
        set "DOXYGEN=C:\Program Files\doxygen\bin\doxygen.exe"
    ) else (
        echo ERROR: Doxygen not found. Install with: winget install doxygen
        pause
        exit /b 1
    )
) else (
    set "DOXYGEN=doxygen"
)

echo === KnC Engine Documentation Generator ===
echo.

if exist "docs\engine\html" (
    echo Cleaning previous output...
    rmdir /s /q "docs\engine\html"
)

echo Generating documentation...
"%DOXYGEN%" Doxyfile

if exist "docs\engine\html\index.html" (
    echo.
    echo Documentation generated successfully!
    echo Output: docs\engine\html\
    echo.
    echo Opening in browser...
    start "" "docs\engine\html\index.html"
) else (
    echo ERROR: Generation failed - index.html not found
    pause
    exit /b 1
)
