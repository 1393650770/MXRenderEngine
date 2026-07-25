@echo off
set "EMSDK=D:\Project\GameDevelop\MyRenderer\src\ThirdParty\emsdk"
set "EMSCRIPTEN=%EMSDK%\upstream\emscripten"
set "EMSDK_NODE=%EMSDK%\node\22.16.0_64bit\bin\node.exe"
set "EMSDK_PYTHON=%EMSDK%\python\3.13.3_64bit\python.exe"
set "PATH=%EMSDK%\upstream\emscripten;%EMSDK%\node\22.16.0_64bit\bin;%EMSDK%;%PATH%"

cd /d D:\Project\GameDevelop\MyRenderer

echo === Emscripten SDK Environment ===
echo EMSDK=%EMSDK%
echo EMSCRIPTEN=%EMSCRIPTEN%
echo.
echo === Checking emcc ===
emcc.bat --version 2>&1 | findstr /C:"emcc"
echo.

echo === Configuring xmake for wasm ===
xmake f -p wasm -a wasm32 -m debug -y
if %ERRORLEVEL% NEQ 0 (
    echo === Configure FAILED ===
    exit /b 1
)

echo.
echo === Building MiniGame-HelloTriangle ===
xmake build MiniGame-HelloTriangle
if %ERRORLEVEL% NEQ 0 (
    echo === Build FAILED ===
    exit /b 1
)

echo.
echo === Build SUCCESS ===
echo Output: build\wasm\wasm32\debug\MiniGame-HelloTriangle.js
