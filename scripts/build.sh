#!/usr/bin/env bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

# Default values
TARGET="${1:-all}"
BUILD_TYPE="${2:-Release}"
JOBS=$(get_cpu_count)

show_usage() {
    echo "Usage: $0 [TARGET] [BUILD_TYPE] [OPTIONS]"
    echo ""
    echo "Targets:"
    echo "  engine       Build only the engine library and CLI"
    echo "  distributed  Build only the distributed replication layer"
    echo "  server       Build only the server (depends on engine + distributed)"
    echo "  cli          Build only the Go CLI"
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
    echo "  $0 cli"
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
    rm -rf "$ROOT_DIR/cli/bin"
fi

# Create build directory
mkdir -p "$BUILD_DIR"

# Normalize build type
BUILD_TYPE=$(normalize_build_type "$BUILD_TYPE")

# Function to build Go CLI
build_go_cli() {
    print_header "Building Go CLI"

    local CLI_DIR="$ROOT_DIR/cli"
    local BIN_DIR="$CLI_DIR/bin"

    if [ ! -d "$CLI_DIR" ]; then
        print_error "CLI directory not found: $CLI_DIR"
        return 1
    fi

    # Check if Go is installed
    if ! command -v go &> /dev/null; then
        print_error "Go is not installed. Please install Go to build the CLI."
        return 1
    fi

    print_info "Go version: $(go version)"

    # Create bin directory
    mkdir -p "$BIN_DIR"

    # Build the CLI
    print_info "Building kvstore-cli..."
    if (cd "$CLI_DIR" && go build -o "$BIN_DIR/kvstore-cli" ./cmd/main.go); then
        print_success "Go CLI built successfully!"
        echo ""
        echo "Executable:"
        echo "  CLI: $BIN_DIR/kvstore-cli"
        return 0
    else
        print_error "Go CLI build failed"
        return 1
    fi
}

# Function to configure and build using root CMake
build_from_root() {
    local target=$1

    print_info "Configuring from root ($BUILD_TYPE)"
    if cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON; then
        print_success "Configuration complete"
    else
        print_error "Configuration failed"
        return 1
    fi

    if [ "$target" = "all" ]; then
        print_info "Building all targets with $JOBS parallel jobs"
        if cmake --build "$BUILD_DIR" -j"$JOBS"; then
            print_success "Build complete"
        else
            print_error "Build failed"
            return 1
        fi
    else
        print_info "Building $target with $JOBS parallel jobs"
        if cmake --build "$BUILD_DIR" --target "kv_$target" -j"$JOBS"; then
            print_success "Build complete for $target"
        else
            print_error "Build failed for $target"
            return 1
        fi
    fi
}

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
        build_from_root engine_core
        print_success "Engine built successfully!"
        echo ""
        echo "Executables:"
        echo "  CLI:    $BUILD_DIR/engine/kv_engine"
        echo "  Tests:  $BUILD_DIR/engine/kv_engine_tests"
        ;;

    distributed)
        print_header "Building Distributed Layer"
        build_from_root distributed
        print_success "Distributed layer built successfully!"
        echo ""
        echo "Library:"
        echo "  $BUILD_DIR/distributed/libkv_distributed.a"
        ;;

    server)
        print_header "Building Server"
        build_from_root server
        print_success "Server built successfully!"
        echo ""
        echo "Executables:"
        echo "  Server: $BUILD_DIR/server/kv_server"
        ;;

    cli)
        build_go_cli
        ;;

    all)
        print_header "Building All Components"
        build_from_root all
        echo ""
        build_go_cli
        echo ""
        print_success "All components built successfully!"
        echo ""
        echo "Executables:"
        echo "  Engine:      $BUILD_DIR/engine/kv_engine"
        echo "  Server:      $BUILD_DIR/server/kv_server"
        echo "  Tests:       $BUILD_DIR/engine/kv_engine_tests"
        echo "  Go CLI:      $ROOT_DIR/cli/bin/kvstore-cli"
        echo ""
        echo "Libraries:"
        echo "  Engine:      $BUILD_DIR/engine/libkv_engine_core.a"
        echo "  Distributed: $BUILD_DIR/distributed/libkv_distributed.a"
        ;;

    tests)
        print_header "Building and Running Tests"
        build_from_root engine_core
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
        build_from_root engine_core
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
