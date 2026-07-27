#!/usr/bin/env bash
set -e

# Default ports (overridable via environment variables)
API_PORT="${API_PORT:-8080}"
FRONTEND_PORT="${FRONTEND_PORT:-3000}"

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

echo "==> Building API server..."
cmake --build --preset conan-release --target garraiobide_app

echo "==> Starting API server on port $API_PORT..."
"$BUILD_PATH/src/garraiobide_app" --port "$API_PORT" &
API_PID=$!

echo "==> Starting frontend server on port $FRONTEND_PORT..."
python3 -m http.server "$FRONTEND_PORT" --directory "$PROJECT_ROOT/frontend" &
FRONTEND_PID=$!

echo ""
echo "  API server:      http://localhost:$API_PORT"
echo "  Frontend server:  http://localhost:$FRONTEND_PORT"
echo ""
echo "Press Ctrl+C to stop both servers."

# Cleanup function to kill both background processes
cleanup() {
    echo ""
    echo "==> Shutting down..."
    kill "$API_PID" 2>/dev/null || true
    kill "$FRONTEND_PID" 2>/dev/null || true
    wait "$API_PID" 2>/dev/null || true
    wait "$FRONTEND_PID" 2>/dev/null || true
    echo "==> Done."
}

trap cleanup SIGINT SIGTERM

# Wait for either process to exit
wait
