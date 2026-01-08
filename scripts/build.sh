#!/usr/bin/env bash
set -e

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"

# Default values
TARGET="${1:-all}"
BUILD_TYPE="${2:-Release}"
JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_header() {
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BLUE}  $1${NC}"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
}

print_success() {
    echo -e "${GREEN}✓${NC} $1"
}

print_error() {
    echo -e "${RED}✗${NC} $1"
}

print_info() {
    echo -e "${YELLOW}→${NC} $1"
}

cmake_configure() {
    local name=$1
    local src=$2
    local out="$BUILD_DIR/$name"

    print_info "Configuring $name ($BUILD_TYPE)"
    if cmake -S "$src" -B "$out" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON; then
        print_success "Configuration complete for $name"
    else
        print_error "Configuration failed for $name"
        return 1
    fi

    print_info "Building $name with $JOBS parallel jobs"
    if cmake --build "$out" -j"$JOBS"; then
        print_success "Build complete for $name"
    else
        print_error "Build failed for $name"
        return 1
    fi
}

show_usage() {
    echo "Usage: $0 [TARGET] [BUILD_TYPE] [OPTIONS]"
    echo ""
    echo "Targets:"
    echo "  engine       Build only the engine library and CLI"
    echo "  server       Build only the server (depends on engine)"
    echo "  all          Build everything (default)"
    echo "  tests        Build and run engine tests"
    echo "  benchmarks   Build engine benchmarks"
    echo ""
    echo "Build Types:"
    echo "  Debug        Debug build with symbols"
    echo "  Release      Optimized release build (default)"
    echo "  RelWithDebInfo  Release with debug info"
    echo "  MinSizeRel   Minimum size release"
    echo ""
    echo "Options:"
    echo "  --help, -h   Show this help message"
    echo "  --clean      Clean before building"
    echo "  --verbose    Verbose build output"
    echo ""
    echo "Examples:"
    echo "  $0 engine Debug"
    echo "  $0 all Release --clean"
    echo "  $0 tests"
}

# Parse additional options
CLEAN_FIRST=false
VERBOSE=false

for arg in "$@"; do
    case "$arg" in
        --help|-h)
            show_usage
            exit 0
            ;;
        --clean)
            CLEAN_FIRST=true
            ;;
        --verbose)
            VERBOSE=true
            set -x
            ;;
    esac
done

# Clean if requested
if [ "$CLEAN_FIRST" = true ]; then
    print_info "Cleaning build directory first"
    rm -rf "$BUILD_DIR"
fi

# Create build directory
mkdir -p "$BUILD_DIR"

print_header "KV Storage Build System"
echo "Root Dir:    $ROOT_DIR"
echo "Build Dir:   $BUILD_DIR"
echo "Target:      $TARGET"
echo "Build Type:  $BUILD_TYPE"
echo "Parallel:    $JOBS jobs"
echo ""

case "$TARGET" in
    engine)
        print_header "Building Engine"
        cmake_configure engine "$ROOT_DIR/engine"
        print_success "Engine built successfully!"
        echo ""
        echo "Executables:"
        echo "  CLI:    $BUILD_DIR/engine/kv_engine"
        echo "  Tests:  $BUILD_DIR/engine/kv_engine_tests"
        ;;
    server)
        print_header "Building Server"
        cmake_configure server "$ROOT_DIR/server"
        print_success "Server built successfully!"
        echo ""
        echo "Executables:"
        echo "  Server: $BUILD_DIR/server/kv_server"
        ;;
    all)
        print_header "Building Engine"
        cmake_configure engine "$ROOT_DIR/engine"
        echo ""
        print_header "Building Server"
        cmake_configure server "$ROOT_DIR/server"
        echo ""
        print_success "All components built successfully!"
        echo ""
        echo "Executables:"
        echo "  Engine: $BUILD_DIR/engine/kv_engine"
        echo "  Server: $BUILD_DIR/server/kv_server"
        echo "  Tests:  $BUILD_DIR/engine/kv_engine_tests"
        ;;
    tests)
        print_header "Building and Running Tests"
        cmake_configure engine "$ROOT_DIR/engine"
        echo ""
        print_info "Running tests..."
        if (cd "$BUILD_DIR/engine" && ctest --output-on-failure); then
            print_success "All tests passed!"
        else
            print_error "Some tests failed"
            exit 1
        fi
        ;;
    benchmarks|bench)
        print_header "Building Benchmarks"
        cmake_configure engine "$ROOT_DIR/engine"
        echo ""
        print_info "Building benchmark targets..."
        if cmake --build "$BUILD_DIR/engine" --target engine_bench -j"$JOBS"; then
            print_success "Benchmarks built successfully!"
            echo ""
            echo "Available benchmarks in $BUILD_DIR/engine:"
            find "$BUILD_DIR/engine" -name "benchmark_*" -type f -executable
        else
            print_error "Benchmark build failed"
            exit 1
        fi
        ;;
    *)
        print_error "Unknown target: $TARGET"
        echo ""
        show_usage
        exit 1
        ;;
esac
