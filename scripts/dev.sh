#!/usr/bin/env bash
set -e

# Default ports (overridable via environment variables)
API_PORT="${API_PORT:-8080}"
FRONTEND_PORT="${FRONTEND_PORT:-3000}"

# Determine project root (script assumes execution from project root)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "==> Building API server..."
bazel build //src/server:api_server

echo "==> Starting API server on port $API_PORT..."
"$PROJECT_ROOT/bazel-bin/src/server/api_server" --port "$API_PORT" &
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
