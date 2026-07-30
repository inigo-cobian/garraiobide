#!/usr/bin/env bash
set -e

# Determine project root (script assumes execution from project root)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# --- Parse arguments ---
COVERAGE=false
OPEN_REPORT=false
for arg in "$@"; do
    case "$arg" in
        --coverage) COVERAGE=true ;;
        --open)     OPEN_REPORT=true ;;
        --help|-h)
            echo "Usage: $0 [--coverage] [--open]"
            echo ""
            echo "  --coverage  Build with coverage instrumentation, run tests,"
            echo "              and generate an HTML coverage report in coverage/"
            echo "  --open      Open the HTML report in the default browser after"
            echo "              generating it (implies --coverage)"
            exit 0
            ;;
        *)
            echo "Unknown option: $arg"
            echo "Run $0 --help for usage."
            exit 1
            ;;
    esac
done

# --open implies --coverage
if [ "$OPEN_REPORT" = true ]; then
    COVERAGE=true
fi

# --- Select build mode ---
if [ "$COVERAGE" = true ]; then
    BUILD_TYPE="Debug"
    BUILD_DIR="${BUILD_DIR:-build/Debug}"
    COVERAGE_DIR="$PROJECT_ROOT/coverage"
else
    BUILD_TYPE="Release"
    BUILD_DIR="${BUILD_DIR:-build/Release}"
fi

BUILD_PATH="$PROJECT_ROOT/$BUILD_DIR"

# --- Install Conan dependencies if needed ---
# With cmake_layout, conan nests generators inside the output folder
CONAN_GENERATORS="$BUILD_PATH/build/$BUILD_TYPE/generators"
CONAN_TOOLCHAIN="$CONAN_GENERATORS/conan_toolchain.cmake"
if [ ! -f "$CONAN_TOOLCHAIN" ]; then
    echo "==> Installing Conan dependencies ($BUILD_TYPE)..."
    conan install "$PROJECT_ROOT" --output-folder="$BUILD_PATH" --build=missing -s build_type="$BUILD_TYPE"
    # Locate the generated toolchain (cmake_layout may nest it)
    CONAN_TOOLCHAIN=$(find "$BUILD_PATH" -name "conan_toolchain.cmake" -print -quit)
    CONAN_GENERATORS=$(dirname "$CONAN_TOOLCHAIN")
fi

# --- Configure CMake ---
if [ ! -f "$BUILD_PATH/Makefile" ] && [ ! -f "$BUILD_PATH/build.ninja" ]; then
    echo "==> Configuring CMake ($BUILD_TYPE)..."
    if [ "$COVERAGE" = true ]; then
        cmake -S "$PROJECT_ROOT" -B "$BUILD_PATH" \
            -DCMAKE_BUILD_TYPE=Debug \
            -DCMAKE_TOOLCHAIN_FILE="$CONAN_TOOLCHAIN" \
            -DCMAKE_CXX_FLAGS="--coverage -fprofile-update=atomic" \
            -DCMAKE_EXE_LINKER_FLAGS="--coverage" \
            -DCMAKE_POLICY_DEFAULT_CMP0091=NEW
    else
        cmake --preset conan-release
    fi
fi

# --- Build ---
echo "==> Building tests..."
if [ "$COVERAGE" = true ]; then
    cmake --build "$BUILD_PATH" --target garraiobide_tests
else
    cmake --build --preset conan-release --target garraiobide_tests
fi

# --- Run tests ---
echo "==> Running tests..."
ctest --test-dir "$BUILD_PATH" --output-on-failure --exclude-regex "Mongo(Integration|Property)Test"

# --- Coverage report ---
if [ "$COVERAGE" = true ]; then
    echo "==> Generating coverage report..."

    # Check that lcov is available
    if ! command -v lcov &>/dev/null; then
        echo "ERROR: lcov is not installed. Install it with:"
        echo "  sudo apt-get install lcov   # Debian/Ubuntu"
        echo "  brew install lcov           # macOS"
        exit 1
    fi

    # Determine the correct gcov tool matching the compiler version
    CXX_VERSION=$(g++ -dumpversion | cut -d. -f1)
    if command -v "gcov-$CXX_VERSION" &>/dev/null; then
        GCOV_TOOL="gcov-$CXX_VERSION"
    else
        GCOV_TOOL="gcov"
    fi
    echo "    Using $GCOV_TOOL (matching g++ $CXX_VERSION)"

    # Reset counters from previous runs
    lcov --zerocounters --directory "$BUILD_PATH"

    # Re-run tests to capture fresh profiling data
    ctest --test-dir "$BUILD_PATH" --output-on-failure --exclude-regex "Mongo(Integration|Property)Test" || true

    # Capture coverage data
    lcov --capture \
        --directory "$BUILD_PATH" \
        --output-file "$BUILD_PATH/coverage.info" \
        --gcov-tool "$GCOV_TOOL" \
        --ignore-errors mismatch,unused,negative

    # Filter out system headers, test code, and mock adapters
    lcov --remove "$BUILD_PATH/coverage.info" \
        '/usr/*' \
        '*/tests/*' \
        '*/adapters/mock*' \
        '*/.conan2/*' \
        --output-file "$BUILD_PATH/coverage_filtered.info" \
        --ignore-errors mismatch,unused,negative

    # Show summary in terminal
    lcov --list "$BUILD_PATH/coverage_filtered.info" --ignore-errors mismatch,unused,negative

    # Generate HTML report
    rm -rf "$COVERAGE_DIR"
    genhtml "$BUILD_PATH/coverage_filtered.info" \
        --output-directory "$COVERAGE_DIR" \
        --title "garraiobide coverage" \
        --legend

    echo ""
    echo "==> Coverage report generated at: $COVERAGE_DIR/index.html"

    if [ "$OPEN_REPORT" = true ]; then
        if command -v xdg-open &>/dev/null; then
            xdg-open "$COVERAGE_DIR/index.html"
        elif command -v open &>/dev/null; then
            open "$COVERAGE_DIR/index.html"
        else
            echo "    (Could not detect a browser opener; open the file manually)"
        fi
    fi
fi
