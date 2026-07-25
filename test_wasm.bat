@echo off
REM ============================================================
REM  MyRenderer WASM Local Test
REM  Build + serve + open browser, one command.
REM  Usage: test_wasm.bat [SampleName] [debug|release]
REM    e.g.: test_wasm.bat MiniGame-HelloTriangle debug
REM ============================================================
setlocal enabledelayedexpansion

set "SAMPLE=%~1"
if "%SAMPLE%"=="" set "SAMPLE=MiniGame-HelloTriangle"
set "MODE=%~2"
if "%MODE%"=="" set "MODE=debug"

set "ROOT=%~dp0"
set "OUT_DIR=%ROOT%build\wasm\wasm32\%MODE%"
set "TEST_DIR=%ROOT%build\wasm\test"
set "PORT=8000"

echo.
echo ============================================================
echo  MyRenderer WASM Test - %SAMPLE% (%MODE%)
echo ============================================================

REM ---- step 1: build ----
echo [1/3] Building WASM...
call "%ROOT%build_wasm.bat" %MODE%
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Build failed!
    pause & exit /b 1
)

REM ---- step 2: prepare test dir (keep original filenames!) ----
echo [2/3] Preparing test directory...
if exist "%TEST_DIR%" rmdir /s /q "%TEST_DIR%"
mkdir "%TEST_DIR%"

REM Copy with ORIGINAL names - Emscripten JS has the .wasm filename baked in
copy "%OUT_DIR%\%SAMPLE%.js"   "%TEST_DIR%\"  > nul
copy "%OUT_DIR%\%SAMPLE%.wasm" "%TEST_DIR%\"  > nul
copy "%OUT_DIR%\%SAMPLE%.data" "%TEST_DIR%\"  > nul 2> nul

REM Generate index.html on the fly (references the real JS file)
> "%TEST_DIR%\index.html" (
echo ^<!DOCTYPE html^>
echo ^<html^>
echo ^<head^>
echo ^<meta charset="utf-8"^>
echo ^<title^>%SAMPLE% - WASM Test^</title^>
echo ^<style^>
echo   body { margin:0; background:#222; overflow:hidden; font-family:monospace; }
echo   canvas { display:block; width:100vw; height:100vh; }
echo   #log { position:fixed; top:0; left:0; right:0; bottom:60%%; padding:8px;
echo          color:#0f0; font-size:11px; overflow-y:auto; white-space:pre-wrap;
echo          background:rgba(0,0,0,0.85); z-index:10; }
echo ^</style^>
echo ^</head^>
echo ^<body^>
echo   ^<canvas id="canvas"^>^</canvas^>
echo   ^<div id="log"^>^</div^>
echo   ^<script^>
echo     var logEl = document.getElementById('log');
echo     function log(kind, msg) {
echo       var t = new Date().toLocaleTimeString();
echo       var line = '[' + t + ' ' + kind + '] ' + msg;
echo       console.log(line);
echo       logEl.textContent += line + '\n';
echo       logEl.scrollTop = logEl.scrollHeight;
echo     }
echo     log('JS', 'start');
echo
echo     var canvas = document.getElementById('canvas');
echo     function resize() {
echo       canvas.width = window.innerWidth * devicePixelRatio;
echo       canvas.height = window.innerHeight * devicePixelRatio;
echo       log('JS', 'resize ' + canvas.width + 'x' + canvas.height + ' dpr=' + devicePixelRatio);
echo     }
echo     window.addEventListener('resize', resize);
echo     resize();
echo
echo     // Check WebGL 2 support
echo     var gl = canvas.getContext('webgl2');
echo     if (!gl) {
echo       log('ERR', 'WebGL 2 NOT supported! Trying webgl...');
echo       gl = canvas.getContext('webgl') || canvas.getContext('experimental-webgl');
echo     }
echo     if (gl) {
echo       log('JS', 'WebGL: ' + gl.getParameter(gl.VERSION));
echo       log('JS', 'GLSL: ' + gl.getParameter(gl.SHADING_LANGUAGE_VERSION));
echo       log('JS', 'Vendor: ' + gl.getParameter(gl.VENDOR));
echo       log('JS', 'Max tex size: ' + gl.getParameter(gl.MAX_TEXTURE_SIZE));
echo     } else {
echo       log('FATAL', 'NO WebGL available!');
echo     }
echo
echo     var Module = {
echo       canvas: canvas,
echo       print:    function(t) { log('c++', t); },
echo       printErr: function(t) { log('ERR', t); },
echo       onAbort:  function(m) { log('FATAL', m); },
echo       onRuntimeInitialized: function() { log('JS', 'WASM runtime initialized, main() starting...'); },
echo       monitorRunDependencies: function(left) { if (left ^> 0) log('JS', 'Waiting on ' + left + ' dependencies...'); },
echo     };
echo
echo     log('JS', 'Loading %SAMPLE%.js ...');
echo     var s = document.createElement('script');
echo     s.src = '%SAMPLE%.js';
echo     s.onload  = function() { log('JS', '%SAMPLE%.js loaded OK'); };
echo     s.onerror = function() { log('FATAL', 'Failed to load %SAMPLE%.js! Check if file exists.'); };
echo     document.body.appendChild(s);
echo   ^</script^>
echo ^</body^>
echo ^</html^>
)

echo   %TEST_DIR%
dir /b "%TEST_DIR%"

REM ---- step 3: serve ----
echo [3/3] Starting HTTP server at http://localhost:%PORT%
echo   Press Ctrl+C to stop, close this window when done.

REM Kill anything already on our port
for /f "tokens=5" %%a in ('netstat -ano ^| findstr ":%PORT%.*LISTENING" 2^>nul') do (
    echo   Killing existing server on port %PORT% (PID %%a^)...
    taskkill /PID %%a /F > nul 2>&1
)

where python > nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo   Server: python -m http.server %PORT%
    REM Start server in a NEW window so browser opens after it's ready
    start "MyRenderer WASM Server" cmd /c "cd /d %TEST_DIR% && python -m http.server %PORT%"
    REM Brief wait for server to start
    timeout /t 2 /nobreak > nul
    echo   Opening browser...
    start "" http://localhost:%PORT%
    goto :end
)

where npx > nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo   Server: npx serve
    start "MyRenderer WASM Server" cmd /c "cd /d %TEST_DIR% && npx serve . --port %PORT% --no-clipboard"
    timeout /t 2 /nobreak > nul
    start "" http://localhost:%PORT%
    goto :end
)

echo ERROR: No Python or Node.js found. Install one:
echo   https://www.python.org/downloads/
echo Then re-run this script.
pause

:end
endlocal
