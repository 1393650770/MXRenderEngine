#!/bin/bash
# Emscripten SDK environment setup for MyRenderer
# Reads EMSDK path from .xmake/paths.ini or uses vendored emsdk.
# Usage: source emsdk_setup.sh

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
INI_FILE="$SCRIPT_DIR/.xmake/paths.ini"

if [ -f "$INI_FILE" ]; then
    EMSDK=$(grep -i "^EMSDK" "$INI_FILE" | cut -d'=' -f2- | xargs)
    # Resolve relative path
    case "$EMSDK" in
        /*|[A-Za-z]:*) ;;  # absolute
        *) EMSDK="$SCRIPT_DIR/$EMSDK" ;;
    esac
else
    EMSDK="$SCRIPT_DIR/src/ThirdParty/emsdk"
fi

export EMSDK
export EMSCRIPTEN="$EMSDK/upstream/emscripten"
export EMSDK_NODE="$EMSDK/node/22.16.0_64bit/bin/node.exe"
export EMSDK_PYTHON="$EMSDK/python/3.13.3_64bit/python.exe"
export PATH="$EMSDK/upstream/emscripten:$EMSDK/node/22.16.0_64bit/bin:$EMSDK:$PATH"

echo "[emsdk] EMSDK=$EMSDK"
echo "[emsdk] emcc: $("$EMSDK_PYTHON" "$EMSCRIPTEN/emcc.py" --version 2>&1 | head -1)"
