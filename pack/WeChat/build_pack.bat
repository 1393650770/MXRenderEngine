@echo off
REM ============================================================
REM  MyRenderer WeChat Mini-Game Pack Script
REM  Builds wasm sample and packages it for WeChat Developer Tools.
REM  Usage: build_pack.bat <SampleName> [debug|release]
REM    e.g.: build_pack.bat MiniGame-HelloTriangle debug
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
set "PACK_DIR=%PROJ_ROOT%\pack\WeChat\dist\%SAMPLE%_wx"

echo ============================================================
echo  MyRenderer WeChat Mini-Game Packager
echo  Sample: %SAMPLE%  Mode: %MODE%
echo ============================================================

REM --- Step 1: Build wasm ---
echo [1/4] Building wasm...
call "%EMSDK%\emsdk_env.bat" > nul 2>&1
cd /d "%PROJ_ROOT%"
xmake f -p wasm -a wasm32 -m %MODE% -y > nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: xmake configure failed
    exit /b 1
)
xmake build %SAMPLE% > nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: xmake build failed
    exit /b 1
)
echo   Build OK.

REM --- Step 2: Prepare output directory ---
echo [2/4] Preparing package directory...
if exist "%PACK_DIR%" rmdir /s /q "%PACK_DIR%"
mkdir "%PACK_DIR%"

REM --- Step 3: Copy template files ---
echo [3/4] Copying game files...
copy "%PROJ_ROOT%\pack\WeChat\game.js"       "%PACK_DIR%\game.js"       > nul
copy "%PROJ_ROOT%\pack\WeChat\game.json"     "%PACK_DIR%\game.json"     > nul
copy "%PROJ_ROOT%\pack\WeChat\project.config.json" "%PACK_DIR%\project.config.json" > nul

REM --- Step 4: Copy wasm build output (rename .js to mxrender.js) ---
if exist "%OUT_DIR%\%SAMPLE%.js" (
    copy "%OUT_DIR%\%SAMPLE%.js"             "%PACK_DIR%\mxrender.js"   > nul
) else (
    echo WARNING: %OUT_DIR%\%SAMPLE%.js not found
)
if exist "%OUT_DIR%\%SAMPLE%.wasm" (
    copy "%OUT_DIR%\%SAMPLE%.wasm"           "%PACK_DIR%\mxrender.wasm" > nul
) else (
    echo WARNING: %OUT_DIR%\%SAMPLE%.wasm not found
)
if exist "%OUT_DIR%\%SAMPLE%.html" (
    copy "%OUT_DIR%\%SAMPLE%.html"           "%PACK_DIR%\index.html"    > nul
)

echo   Files copied.

REM --- Summary ---
echo ============================================================
echo  Package complete!
echo  Output: %PACK_DIR%
echo.
echo  To test in WeChat Developer Tools:
echo    1. Open WeChat Developer Tools
echo    2. Import project -> Select "%PACK_DIR%"
echo    3. Set AppID or use "Test Account"
echo    4. Click "Compile" to build and preview
echo ============================================================

endlocal
