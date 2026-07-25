@echo off
REM ============================================================
REM  MyRenderer WASM Build Script
REM  Builds all 3 MiniGame samples for browser / WeChat / Douyin.
REM  Usage: build_wasm.bat [debug|release]
REM ============================================================
setlocal enabledelayedexpansion

REM --- Project root (this script's directory) ---
set "PROJ_ROOT=%~dp0"
REM Remove trailing backslash
if "%PROJ_ROOT:~-1%"=="\" set "PROJ_ROOT=%PROJ_ROOT:~0,-1%"

REM --- Read EMSDK from .xmake\paths.ini ---
set "EMSDK=%PROJ_ROOT%\src\ThirdParty\emsdk"
set "INI_FILE=%PROJ_ROOT%\.xmake\paths.ini"
if exist "%INI_FILE%" (
    for /f "tokens=2 delims== " %%a in ('findstr /b "EMSDK" "%INI_FILE%" 2^>nul') do set "EMSDK=%%a"
)
REM Resolve relative path against project root
if not "%EMSDK:~1,1%"==":" if not "%EMSDK:~0,1%"=="\\" (
    set "EMSDK=%PROJ_ROOT%\%EMSDK%"
)

set "EMSCRIPTEN=%EMSDK%\upstream\emscripten"
set "EMSDK_NODE=%EMSDK%\node\22.16.0_64bit\bin\node.exe"
set "EMSDK_PYTHON=%EMSDK%\python\3.13.3_64bit\python.exe"
set "PATH=%EMSDK%\upstream\emscripten;%EMSDK%\node\22.16.0_64bit\bin;%EMSDK%;%PATH%"

cd /d "%PROJ_ROOT%"

echo === Emscripten SDK Environment ===
echo EMSDK=%EMSDK%
echo EMSCRIPTEN=%EMSCRIPTEN%
echo.
echo === Checking emcc ===
emcc.bat --version 2>&1 | findstr /C:"emcc"
echo.

set "MODE=%~1"
if "%MODE%"=="" set "MODE=debug"

echo === Configuring xmake for wasm (%MODE%) ===
xmake f -p wasm -a wasm32 -m %MODE% -y
if %ERRORLEVEL% NEQ 0 (
    echo === Configure FAILED ===
    exit /b 1
)

for %%S in (MiniGame-HelloTriangle MiniGame-Texture MiniGame-Mesh) do (
    echo.
    echo === Building %%S ===
    xmake build %%S
    if %ERRORLEVEL% NEQ 0 (
        echo === %%S FAILED ===
        exit /b 1
    )
)

echo.
echo ============================================
echo  All 3 MiniGame samples built successfully!
echo  Output: build\wasm\wasm32\%MODE%\
echo  Serve:  python -m http.server 8080
echo  Open:   http://localhost:8080/MiniGame-Mesh.html
echo ============================================
endlocal
