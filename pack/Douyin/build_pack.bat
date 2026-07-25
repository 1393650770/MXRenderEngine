@echo off
REM ============================================================
REM  MyRenderer Douyin Mini-Game Pack Script
REM  Usage: build_pack.bat <SampleName> [debug|release]
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

set "EMSDK=D:\Project\GameDevelop\MyRenderer\src\ThirdParty\emsdk"
set "PROJ_ROOT=D:\Project\GameDevelop\MyRenderer"
set "OUT_DIR=%PROJ_ROOT%\build\wasm\wasm32\%MODE%"
set "PACK_DIR=%PROJ_ROOT%\pack\Douyin\dist\%SAMPLE%_tt"

echo ============================================================
echo  MyRenderer Douyin Mini-Game Packager
echo  Sample: %SAMPLE%  Mode: %MODE%
echo ============================================================

echo [1/4] Building wasm...
call "%EMSDK%\emsdk_env.bat" > nul 2>&1
cd /d "%PROJ_ROOT%"
xmake f -p wasm -a wasm32 -m %MODE% -y > nul 2>&1
if %ERRORLEVEL% NEQ 0 ( echo ERROR: xmake configure failed & exit /b 1 )
xmake build %SAMPLE% > nul 2>&1
if %ERRORLEVEL% NEQ 0 ( echo ERROR: xmake build failed & exit /b 1 )
echo   Build OK.

echo [2/4] Preparing package directory...
if exist "%PACK_DIR%" rmdir /s /q "%PACK_DIR%"
mkdir "%PACK_DIR%"

echo [3/4] Copying game files...
copy "%PROJ_ROOT%\pack\Douyin\game.js"            "%PACK_DIR%\game.js"            > nul
copy "%PROJ_ROOT%\pack\Douyin\game.json"          "%PACK_DIR%\game.json"          > nul
copy "%PROJ_ROOT%\pack\Douyin\project.config.json" "%PACK_DIR%\project.config.json" > nul

if exist "%OUT_DIR%\%SAMPLE%.js"   copy "%OUT_DIR%\%SAMPLE%.js"   "%PACK_DIR%\mxrender.js"   > nul
if exist "%OUT_DIR%\%SAMPLE%.wasm" copy "%OUT_DIR%\%SAMPLE%.wasm" "%PACK_DIR%\mxrender.wasm" > nul

echo   Files copied.

echo ============================================================
echo  Package complete! Output: %PACK_DIR%
echo  To test: Import into Douyin Developer Tools
echo ============================================================
endlocal
