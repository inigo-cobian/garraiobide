#!/usr/bin/env bash
set -e

# Default CMake build directory (overridable via environment variables)
BUILD_DIR="${BUILD_DIR:-build/Release}"

# Determine project root (script assumes execution from project root)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

BUILD_PATH="$PROJECT_ROOT/$BUILD_DIR"

# Install Conan dependencies if generators haven't been produced yet
CONAN_TOOLCHAIN="$BUILD_PATH/generators/conan_toolchain.cmake"
if [ ! -f "$CONAN_TOOLCHAIN" ]; then
    echo "==> Installing Conan dependencies..."
    conan install "$PROJECT_ROOT" --output-folder="$BUILD_PATH" --build=missing -s build_type=Release
fi

# Configure only if the build directory hasn't been fully configured yet
if [ ! -f "$BUILD_PATH/Makefile" ]; then
    echo "==> Configuring CMake in $BUILD_DIR..."
    cmake --preset conan-release
fi

echo "==> Building tests..."
cmake --build --preset conan-release --target garraiobide_tests

echo "==> Running tests..."
ctest --test-dir "$BUILD_PATH" --output-on-failure
