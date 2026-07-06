#!/usr/bin/env bash
# build.sh — Compiles cpp-src/*.cpp into libVouncy.so, linked against the
# WaveGUI native core (SDL2 + Blend2D).
#
# NOTE ON WAVEVM DISCOVERY: I don't know your toolchain's exact convention
# for how WaveVM/WaveCore locates and loads a project's compiled native
# library at runtime (fixed name next to the .wave sources? a path declared
# in Settings.wproj? an explicit `wavec --link` flag?). This script just
# produces libVouncy.so next to this file — wire up whatever loading step
# your wavec pipeline expects on top of that.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Adjust this to wherever wavegui_native's include/ and build/ dirs live.
WAVEGUI_NATIVE_DIR="${WAVEGUI_NATIVE_DIR:-$SCRIPT_DIR/../wavegui_native}"

g++ -std=c++17 -shared -fPIC -O2 \
    -I"$SCRIPT_DIR" \
    -I"$WAVEGUI_NATIVE_DIR/include" \
    "$SCRIPT_DIR/cpp-src/VouncyBridge.cpp" \
    -L"$WAVEGUI_NATIVE_DIR/build" -lwavegui_native \
    -Wl,-rpath,'$ORIGIN' \
    -o "$SCRIPT_DIR/libVouncy.so"

echo "Built: $SCRIPT_DIR/libVouncy.so"
echo "(also needs libwavegui_native.so alongside it at runtime, or statically link that in instead)"