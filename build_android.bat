@echo off
REM ============================================================
REM  MyRenderer Android Build Script
REM  Builds all Android-compatible samples.
REM  Reads NDK path from .xmake\paths.ini (like build_wasm.bat).
REM  Usage: build_android.bat [debug|release]
REM ============================================================
setlocal enabledelayedexpansion

REM --- Project root (this script's directory) ---
set "PROJ_ROOT=%~dp0"
if "%PROJ_ROOT:~-1%"=="\" set "PROJ_ROOT=%PROJ_ROOT:~0,-1%"

REM --- Read NDK from .xmake\paths.ini ---
set "NDK_PATH="
set "INI_FILE=%PROJ_ROOT%\.xmake\paths.ini"
if exist "%INI_FILE%" (
    for /f "tokens=2 delims== " %%a in ('findstr /b "NDK" "%INI_FILE%" 2^>nul') do set "NDK_PATH=%%a"
)
if "%NDK_PATH%"=="" (
    echo ERROR: Cannot read NDK path from .xmake\paths.ini
    echo   Set: NDK = D:/path/to/android-ndk
    exit /b 1
)

set "MODE=%~1"
if "%MODE%"=="" set "MODE=debug"

echo ============================================================
echo  MyRenderer Android Build
echo  NDK: %NDK_PATH%
echo  Mode: %MODE%
echo ============================================================

cd /d "%PROJ_ROOT%"

echo.
echo === Configuring xmake for Android ===
xmake f -p android -a arm64-v8a --ndk="%NDK_PATH%" --ndk_sdkver=26 -m %MODE% -y
if %ERRORLEVEL% NEQ 0 (
    echo === Configure FAILED ===
    exit /b 1
)

for %%S in (RendererSample-HelloTriangle RendererSample-Texture RendererSample-Mesh RendererSample-Ocean) do (
    echo.
    echo === Building %%S ===
    xmake build %%S
    if %ERRORLEVEL% NEQ 0 (
        echo === %%S FAILED ===
        REM Continue building other samples
    )
)

echo.
echo ============================================
echo  Android build finished!
echo  Output: build\android\arm64-v8a\%MODE%\
echo.
echo  Package APK: pack\Android\build_apk.bat
echo ============================================
endlocal
