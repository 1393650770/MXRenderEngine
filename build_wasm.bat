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
