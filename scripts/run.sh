#!/usr/bin/env bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

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

while [ $# -gt 0 ]; do
    case "$1" in
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
            shift
            BENCHMARK_NAME="$1"
            ;;
        *)
            print_error "Unknown option: $1"
            show_usage
            exit 1
            ;;
    esac
    shift
done

# Build if requested
if [ "$BUILD_FIRST" = true ]; then
    print_info "Building first..."
    "$SCRIPT_DIR/build.sh" "$TARGET"
    echo ""
fi

# Determine executable path
EXECUTABLE=$(get_executable_path "$TARGET" "$BENCHMARK_NAME")
WORKDIR=$(get_working_dir "$TARGET")

# Handle benchmark target specifically
if [ "$TARGET" = "benchmark" ]; then
    if [ -z "$BENCHMARK_NAME" ]; then
        print_error "Benchmark name required. Use --benchmark NAME"
        echo ""
        echo "Available benchmarks:"
        find "$BUILD_DIR/engine" -name "benchmark_*" -type f -executable 2>/dev/null | xargs -n1 basename || echo "  None found. Build first?"
        exit 1
    fi

    if [ -z "$EXECUTABLE" ]; then
        print_error "Invalid benchmark target"
        exit 1
    fi
fi

# Check if executable exists
if ! check_executable "$EXECUTABLE" "$TARGET"; then
    echo "Run with --build to build first, or run: $SCRIPT_DIR/build.sh $TARGET"
    exit 1
fi

# Change to working directory
cd "$WORKDIR"

# Run with appropriate wrapper
if [ "$USE_VALGRIND" = true ]; then
    if ! require_command valgrind "  macOS:  brew install valgrind
  Ubuntu: sudo apt-get install valgrind"; then
        exit 1
    fi
    print_info "Running with valgrind..."
    valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes "$EXECUTABLE"

elif [ "$USE_GDB" = true ]; then
    if ! require_command gdb "  macOS:  brew install gdb
  Ubuntu: sudo apt-get install gdb"; then
        exit 1
    fi
    print_info "Running with gdb..."
    gdb "$EXECUTABLE"

else
    print_info "Running $TARGET..."
    "$EXECUTABLE"
fi
