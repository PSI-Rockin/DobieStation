#!/bin/sh

CMAKE="cmake"
BUILD_DIR="build-clang"
TOOLCHAIN="./cmake/Toolchain-clang-linux-native.cmake"

exec $CMAKE . -B "$BUILD_DIR" -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" "$@"
