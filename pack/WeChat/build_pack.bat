@echo off
REM ============================================================
REM  MyRenderer WeChat Mini-Game Pack Script
REM  Builds wasm sample and packages for WeChat Developer Tools.
REM  Usage: build_pack.bat <SampleName> [debug|release]
REM    e.g.: build_pack.bat MiniGame-Mesh debug
REM ============================================================
setlocal enabledelayedexpansion

set "SAMPLE=%~1"
if "%SAMPLE%"=="" (
    echo Usage: build_pack.bat <SampleName> [debug|release]
    echo   Available: MiniGame-HelloTriangle, MiniGame-Texture, MiniGame-Mesh
    exit /b 1
)

set "MODE=%~2"
if "%MODE%"=="" set "MODE=debug"

REM --- Resolve paths relative to this script ---
set "SCRIPT_DIR=%~dp0"
set "PROJ_ROOT=%SCRIPT_DIR%..\.."
pushd "%PROJ_ROOT%" && set "PROJ_ROOT=%CD%" && popd

set "EMSDK=%PROJ_ROOT%\src\ThirdParty\emsdk"
set "OUT_DIR=%PROJ_ROOT%\build\wasm\wasm32\%MODE%"
set "PACK_DIR=%SCRIPT_DIR%dist\%SAMPLE%_wx"

echo ============================================================
echo  MyRenderer WeChat Mini-Game Packager
echo  Sample: %SAMPLE%  Mode: %MODE%
echo ============================================================

REM --- Step 1: Build wasm (using project's build_wasm.bat) ---
echo [1/3] Building wasm...
call "%PROJ_ROOT%\build_wasm.bat" %MODE%
if %ERRORLEVEL% NEQ 0 ( echo ERROR: Build failed & exit /b 1 )
echo   Build OK.

REM --- Step 2: Prepare output directory ---
echo [2/3] Preparing package directory...
if exist "%PACK_DIR%" rmdir /s /q "%PACK_DIR%"
mkdir "%PACK_DIR%"

REM --- Step 3: Copy files ---
echo [3/3] Copying game files...
copy "%SCRIPT_DIR%game.js"              "%PACK_DIR%\game.js"            > nul
copy "%SCRIPT_DIR%game.json"            "%PACK_DIR%\game.json"          > nul
copy "%SCRIPT_DIR%project.config.json"  "%PACK_DIR%\project.config.json" > nul

if exist "%OUT_DIR%\%SAMPLE%.js"   copy "%OUT_DIR%\%SAMPLE%.js"   "%PACK_DIR%\mxrender.js"   > nul
if exist "%OUT_DIR%\%SAMPLE%.wasm" copy "%OUT_DIR%\%SAMPLE%.wasm" "%PACK_DIR%\mxrender.wasm" > nul

echo   Files copied.

echo ============================================================
echo  Package complete!
echo  Output: %PACK_DIR%
echo.
echo  To test in WeChat Developer Tools:
echo    1. Open WeChat Developer Tools
echo    2. Import project -^> Select "%PACK_DIR%"
echo    3. Set AppID or use "Test Account"
echo    4. Click "Compile" to build and preview
echo ============================================================
endlocal
