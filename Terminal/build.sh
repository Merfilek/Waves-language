#!/usr/bin/env bash
# build.sh — builds Helper.{so,dylib,dll} for whichever platform you run this
# on, or cross-compiles for Windows from Linux when CROSS_MINGW=1.
#
# This library doesn't call waves_array_create/waves_object_create (the
# VM-populated bridge symbols), so it links standalone -- no import library
# needed on any platform.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

if [ "${CROSS_MINGW:-0}" = "1" ]; then
    x86_64-w64-mingw32-g++ -std=c++17 -shared -O2 \
        -static-libgcc -static-libstdc++ -static \
        -I. Helper.cpp -o Helper.dll
    echo "Built: $SCRIPT_DIR/Helper.dll (cross-compiled for Windows)"
elif [ "$(uname -s)" = "Darwin" ]; then
    clang++ -std=c++17 -dynamiclib -O2 -I. Helper.cpp -o Helper.dylib
    echo "Built: $SCRIPT_DIR/Helper.dylib"
elif [ "$(uname -s)" = "Linux" ]; then
    g++ -std=c++17 -shared -fPIC -O2 -I. Helper.cpp -o Helper.so
    echo "Built: $SCRIPT_DIR/Helper.so"
else
    echo "Unsupported platform: $(uname -s)" >&2
    exit 1
fi
