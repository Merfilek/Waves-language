#!/usr/bin/env bash
# build.sh — builds Helper.{so,dylib,dll} for whichever platform you run this
# on, or cross-compiles for Windows from Linux when CROSS_MINGW=1.
#
# This library calls waves_array_create/waves_array_push -- extern function-
# pointer variables the VM (wavm/wavecore) sets at startup, resolved against
# whatever process loads this library. Linux/macOS do that resolution
# automatically (dlopen(RTLD_GLOBAL)'s normal symbol interposition); a
# Windows DLL needs an explicit import library naming the exact module the
# symbol comes from, generated from wavm.exe's own exports
# (WaveVM/CMakeLists.txt's -Wl,--out-implib -- see its comment). Point
# WAVM_WIN_IMPLIB at that libwavm.dll.a if it isn't at the default relative
# path below (e.g. `WaveVM/build-win-x86_64/libwavm.dll.a` from WaveVM's own
# `./build-cross.sh`).
#
# NOTE: this only covers loading through wavm (the interpreter) -- WaveCore's
# AOT model compiles every program to its own uniquely-named .exe, so a
# prebuilt Helper.dll can't link against one fixed import library the way it
# can for wavm.exe. That needs its own fix; see the memory note on this.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

if [ "${CROSS_MINGW:-0}" = "1" ]; then
    IMPLIB="${WAVM_WIN_IMPLIB:-$SCRIPT_DIR/../../Waves/WaveVM/build-win-x86_64/libwavm.dll.a}"
    if [ ! -f "$IMPLIB" ]; then
        echo "error: wavm's Windows import library not found at $IMPLIB" >&2
        echo "  Build it with: (cd WaveVM && ./build-cross.sh)" >&2
        echo "  Or point WAVM_WIN_IMPLIB at it directly." >&2
        exit 1
    fi
    # --allow-multiple-definition: wavm.exe is linked fully static (-static), so its
    # import library re-exposes libgcc_eh's unwind symbols too -- this library's own
    # static libgcc_eh then collides with that copy. Confirmed harmless: both are the
    # exact same compiler's exact same object, just linked in from two places.
    x86_64-w64-mingw32-g++ -std=c++17 -shared -O2 \
        -static-libgcc -static-libstdc++ -static -Wl,--allow-multiple-definition \
        -I. Helper.cpp "$IMPLIB" -o Helper.dll
    echo "Built: $SCRIPT_DIR/Helper.dll (cross-compiled for Windows, linked against $IMPLIB)"
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
