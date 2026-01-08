#!/usr/bin/env bash
set -e

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_error() {
    echo -e "${RED}✗${NC} $1"
}

print_info() {
    echo -e "${YELLOW}→${NC} $1"
}

print_success() {
    echo -e "${GREEN}✓${NC} $1"
}

show_usage() {
    echo "Usage: $0 [TARGET] [OPTIONS]"
    echo ""
    echo "Targets:"
    echo "  engine       Run the engine CLI"
    echo "  server       Run the server"
    echo "  tests        Run engine tests"
    echo "  benchmark    Run a specific benchmark"
    echo ""
    echo "Options:"
    echo "  --help, -h          Show this help message"
    echo "  --build             Build before running"
    echo "  --valgrind          Run with valgrind memory checking"
    echo "  --gdb               Run with gdb debugger"
    echo "  --benchmark NAME    Run specific benchmark (use with 'benchmark' target)"
    echo ""
    echo "Examples:"
    echo "  $0 engine"
    echo "  $0 server --build"
    echo "  $0 tests --valgrind"
    echo "  $0 benchmark --benchmark bloom_filter"
}

# Parse arguments
TARGET="${1:-engine}"
BUILD_FIRST=false
USE_VALGRIND=false
USE_GDB=false
BENCHMARK_NAME=""

shift || true
for arg in "$@"; do
    case "$arg" in
        --help|-h)
            show_usage
            exit 0
            ;;
        --build)
            BUILD_FIRST=true
            ;;
        --valgrind)
            USE_VALGRIND=true
            ;;
        --gdb)
            USE_GDB=true
            ;;
        --benchmark)
            shift || true
            BENCHMARK_NAME="$1"
            shift || true
            ;;
    esac
done

# Build if requested
if [ "$BUILD_FIRST" = true ]; then
    print_info "Building first..."
    "$ROOT_DIR/scripts/build.sh" "$TARGET"
    echo ""
fi

# Determine executable path
case "$TARGET" in
    engine)
        EXECUTABLE="$BUILD_DIR/engine/kv_engine"
        WORKDIR="$ROOT_DIR/engine"
        ;;
    server)
        EXECUTABLE="$BUILD_DIR/server/kv_server"
        WORKDIR="$ROOT_DIR/server"
        ;;
    tests)
        EXECUTABLE="$BUILD_DIR/engine/kv_engine_tests"
        WORKDIR="$ROOT_DIR/engine"
        ;;
    benchmark)
        if [ -z "$BENCHMARK_NAME" ]; then
            print_error "Benchmark name required. Use --benchmark NAME"
            echo ""
            echo "Available benchmarks:"
            find "$BUILD_DIR/engine" -name "benchmark_*" -type f -executable 2>/dev/null | xargs -n1 basename || echo "  None found. Build first?"
            exit 1
        fi
        EXECUTABLE="$BUILD_DIR/engine/benchmark_$BENCHMARK_NAME"
        WORKDIR="$ROOT_DIR/engine"
        ;;
    *)
        print_error "Unknown target: $TARGET"
        show_usage
        exit 1
        ;;
esac

# Check if executable exists
if [ ! -f "$EXECUTABLE" ]; then
    print_error "Executable not found: $EXECUTABLE"
    echo "Run with --build to build first, or run: ./scripts/build.sh $TARGET"
    exit 1
fi

# Change to working directory
cd "$WORKDIR"

# Run with appropriate wrapper
if [ "$USE_VALGRIND" = true ]; then
    if ! command -v valgrind &> /dev/null; then
        print_error "valgrind not found. Install it first."
        exit 1
    fi
    print_info "Running with valgrind..."
    valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes "$EXECUTABLE"
elif [ "$USE_GDB" = true ]; then
    if ! command -v gdb &> /dev/null; then
        print_error "gdb not found. Install it first."
        exit 1
    fi
    print_info "Running with gdb..."
    gdb "$EXECUTABLE"
else
    print_info "Running $TARGET..."
    "$EXECUTABLE"
fi
