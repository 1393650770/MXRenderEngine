#!/bin/bash
# Emscripten SDK environment setup for MyRenderer
# Source this before building: source emsdk_setup.sh

EMSDK_ROOT="/d/Project/GameDevelop/MyRenderer/src/ThirdParty/emsdk"

export EMSDK="$EMSDK_ROOT"
export EMSCRIPTEN="$EMSDK_ROOT/upstream/emscripten"
export EMSDK_NODE="$EMSDK_ROOT/node/22.16.0_64bit/bin/node.exe"
export EMSDK_PYTHON="$EMSDK_ROOT/python/3.13.3_64bit/python.exe"
export PATH="$EMSDK_ROOT/upstream/emscripten:$EMSDK_ROOT/node/22.16.0_64bit/bin:$EMSDK_ROOT:$PATH"

echo "[emsdk] Environment set up: EMSDK=$EMSDK"
echo "[emsdk] emcc version: $("$EMSDK_PYTHON" "$EMSCRIPTEN/emcc.py" --version 2>&1 | head -1)"
