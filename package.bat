@echo off
REM ── Wave Packaging Script ────────────────────────────────────
REM Creates a clean release folder ready for distribution.
REM Usage: package.bat [Release|Debug]
REM Output: dist\Wave\

setlocal
set CONFIG=%~1
if "%CONFIG%"=="" set CONFIG=Release

set BUILD_DIR=build\%CONFIG%
set DIST_DIR=dist\Wave

echo [Wave] Packaging %CONFIG% build...

if not exist "%BUILD_DIR%\Wave.exe" (
    echo ERROR: %BUILD_DIR%\Wave.exe not found. Build first:
    echo   cmake -B build -A x64
    echo   cmake --build build --config %CONFIG%
    exit /b 1
)

REM Clean and create dist folder
if exist "%DIST_DIR%" rmdir /s /q "%DIST_DIR%"
mkdir "%DIST_DIR%"
mkdir "%DIST_DIR%\plugins"

REM Copy main executable
copy "%BUILD_DIR%\Wave.exe" "%DIST_DIR%\" >nul

REM Copy libmpv DLL (try both names)
if exist "%BUILD_DIR%\libmpv-2.dll" copy "%BUILD_DIR%\libmpv-2.dll" "%DIST_DIR%\" >nul
if exist "%BUILD_DIR%\mpv-2.dll" copy "%BUILD_DIR%\mpv-2.dll" "%DIST_DIR%\" >nul

REM Copy plugins
if exist "%BUILD_DIR%\plugins\*.dll" copy "%BUILD_DIR%\plugins\*.dll" "%DIST_DIR%\plugins\" >nul

REM Copy SDK header (for plugin developers)
if not exist "%DIST_DIR%\sdk" mkdir "%DIST_DIR%\sdk"
copy "sdk\wave_plugin_sdk.h" "%DIST_DIR%\sdk\" >nul

REM Create portable marker (optional — remove for installed mode)
echo This file enables portable mode. Settings are stored in .\data\ > "%DIST_DIR%\portable.txt"

echo.
echo [Wave] Package complete: %DIST_DIR%\
echo Contents:
dir /b "%DIST_DIR%"
echo.
echo To run: %DIST_DIR%\Wave.exe
echo To create installer: iscc installer.iss
