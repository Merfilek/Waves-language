#!/bin/bash
# WavesUI build script

CC=g++
FLAGS="-std=c++17 -O2 -fPIC -Wall -Wno-unused-parameter"
INCLUDES="-I./include -I./vendor -I./src"
LIBS="-lglfw -lnanovg -lGL -ldl -lm -lpthread"

# Source files (added src/glad.c here)
SOURCES=(
    src/glad.c
    src/CSSParser.cpp
    src/Widget.cpp
    src/Layout.cpp
    src/EventSystem.cpp
    src/Renderer.cpp
    src/WindowManager.cpp
    src/WavesUI.cpp
)

BUILD_DIR=build
OUT="$BUILD_DIR/Helper.so"

mkdir -p "$BUILD_DIR"

echo "[WavesUI] Compiling..."

OBJECTS=()
for SRC in "${SOURCES[@]}"; do
    # Get filename without extension
    FILENAME=$(basename "$SRC")
    OBJ="$BUILD_DIR/${FILENAME%.*}.o"
    
    echo "  CC $SRC → $OBJ"
    
    # We use $CC for both, g++ handles .c files just fine
    $CC $FLAGS $INCLUDES -c "$SRC" -o "$OBJ"
    OBJECTS+=("$OBJ")
done

echo "[WavesUI] Linking → $OUT"
$CC -shared -fPIC -o "$OUT" "${OBJECTS[@]}" $LIBS

echo "[WavesUI] Done! Library at: $OUT"