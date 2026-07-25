@echo off
REM ============================================================
REM  MyRenderer Douyin Mini-Game Pack Script
REM  Usage: build_pack.bat <SampleName> [debug|release]
REM ============================================================
setlocal enabledelayedexpansion

set "SAMPLE=%~1"
if "%SAMPLE%"=="" (
    echo Usage: build_pack.bat ^<SampleName^> [debug|release]
    echo   Available: MiniGame-HelloTriangle, MiniGame-Texture, MiniGame-Mesh
    exit /b 1
)
set "MODE=%~2"
if "%MODE%"=="" set "MODE=debug"

REM Resolve project root: this script is in pack\Douyin\, project is two levels up
set "PROJ_ROOT=%~dp0..\.."
set "OUT_DIR=%PROJ_ROOT%\build\wasm\wasm32\%MODE%"
set "PACK_DIR=%~dp0dist\%SAMPLE%_tt"

echo ============================================================
echo  MyRenderer Douyin Mini-Game Packager
echo  Sample: %SAMPLE%  Mode: %MODE%
echo  Pack:   %PACK_DIR%
echo ============================================================

echo [1/2] Building wasm...
call "%PROJ_ROOT%\build_wasm.bat" %MODE%
if %ERRORLEVEL% NEQ 0 ( echo ERROR: Build failed & exit /b 1 )

echo [2/2] Preparing package...
if exist "%PACK_DIR%" rmdir /s /q "%PACK_DIR%"
mkdir "%PACK_DIR%"

copy "%~dp0game.js"             "%PACK_DIR%\game.js"   > nul
copy "%~dp0app.json"            "%PACK_DIR%\app.json"  > nul
copy "%~dp0project.config.json" "%PACK_DIR%\project.config.json" > nul

if exist "%OUT_DIR%\%SAMPLE%.js"   copy "%OUT_DIR%\%SAMPLE%.js"   "%PACK_DIR%\mxrender.js"   > nul
if exist "%OUT_DIR%\%SAMPLE%.wasm" copy "%OUT_DIR%\%SAMPLE%.wasm" "%PACK_DIR%\mxrender.wasm" > nul

echo Done. Import "%PACK_DIR%" into Douyin Developer Tools.
endlocal
