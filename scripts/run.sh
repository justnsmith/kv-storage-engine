#!/usr/bin/env bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

show_usage() {
    echo "Usage: $0 [TARGET] [OPTIONS] [-- ARGS]"
    echo ""
    echo "Targets:"
    echo "  engine       Run the engine CLI"
    echo "  server       Run the server"
    echo "  cli          Run the Go CLI"
    echo "  tests        Run engine tests"
    echo "  benchmark    Run a specific benchmark"
    echo "  cluster      Run the local 3-node cluster"
    echo ""
    echo "Docker Targets:"
    echo "  docker:build     Build Docker image (--no-cache, --plain)"
    echo "  docker:up        Start Docker cluster"
    echo "  docker:down      Stop Docker cluster (--clean, --remove-images)"
    echo "  docker:logs      View Docker logs"
    echo "  docker:rebuild   Stop, clean rebuild, and start"
    echo "  docker:clean     Complete cleanup (stop, remove data & images)"
    echo ""
    echo "Options:"
    echo "  --help, -h          Show this help message"
    echo "  --build             Build before running"
    echo "  --valgrind          Run with valgrind memory checking (C++ only)"
    echo "  --gdb               Run with gdb debugger (C++ only)"
    echo "  --benchmark NAME    Run specific benchmark (use with 'benchmark' target)"
    echo ""
    echo "Docker Options (pass after target):"
    echo "  docker:build --no-cache    Force clean rebuild"
    echo "  docker:build --plain       Show detailed build output"
    echo "  docker:down --clean        Remove data volumes"
    echo "  docker:down --remove-images Remove Docker images"
    echo ""
    echo "Pass arguments after '--' to forward them to the target program"
    echo ""
    echo "Examples:"
    echo "  $0 engine"
    echo "  $0 server --build"
    echo "  $0 cli -- get mykey"
    echo "  $0 cli --build -- --host localhost --port 5555 ping"
    echo "  $0 tests --valgrind"
    echo "  $0 benchmark --benchmark bloom_filter"
    echo "  $0 cluster"
    echo "  $0 docker:build --no-cache"
    echo "  $0 docker:down --clean --remove-images"
    echo "  $0 docker:rebuild"
    echo "  $0 docker:logs -- 1"
}

# Parse arguments
TARGET="${1:-engine}"
BUILD_FIRST=false
USE_VALGRIND=false
USE_GDB=false
BENCHMARK_NAME=""
EXTRA_ARGS=()

shift || true

# For Docker commands, collect all remaining args
# They'll be passed directly to the docker scripts
if [[ "$TARGET" == docker:* ]] || [[ "$TARGET" == docker-* ]]; then
    EXTRA_ARGS=("$@")
else
    # Parse options until we hit '--' or run out
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
            --)
                shift
                EXTRA_ARGS=("$@")
                break
                ;;
            *)
                print_error "Unknown option: $1"
                show_usage
                exit 1
                ;;
        esac
        shift
    done
fi

# Handle Docker targets
case "$TARGET" in
    docker:build|docker-build)
        exec "$SCRIPT_DIR/docker-build.sh" "${EXTRA_ARGS[@]}"
        ;;
    docker:up|docker-up)
        exec "$SCRIPT_DIR/docker-up.sh" "${EXTRA_ARGS[@]}"
        ;;
    docker:down|docker-down)
        exec "$SCRIPT_DIR/docker-down.sh" "${EXTRA_ARGS[@]}"
        ;;
    docker:logs|docker-logs)
        exec "$SCRIPT_DIR/docker-logs.sh" "${EXTRA_ARGS[@]}"
        ;;
    docker:rebuild)
        print_header "Rebuilding Docker Images (No Cache)"
        "$SCRIPT_DIR/docker-down.sh"
        exec "$SCRIPT_DIR/docker-build.sh" --no-cache "${EXTRA_ARGS[@]}"
        ;;
    docker:clean)
        print_header "Complete Docker Cleanup"
        exec "$SCRIPT_DIR/docker-down.sh" --clean --remove-images
        ;;
    cluster)
        exec "$SCRIPT_DIR/run_cluster.sh"
        ;;
esac

# Build if requested
if [ "$BUILD_FIRST" = true ]; then
    print_info "Building first..."
    if [ "$TARGET" = "cli" ]; then
        "$SCRIPT_DIR/build.sh" cli
    else
        "$SCRIPT_DIR/build.sh" "$TARGET"
    fi
    echo ""
fi

# Handle Go CLI target
if [ "$TARGET" = "cli" ]; then
    CLI_EXECUTABLE="$BUILD_DIR/cli/kvstore-cli"

    # Check if executable exists
    if [ ! -f "$CLI_EXECUTABLE" ]; then
        print_error "Go CLI not found: $CLI_EXECUTABLE"
        echo "Run with --build to build first, or run: $SCRIPT_DIR/build.sh cli"
        exit 1
    fi

    # Valgrind and GDB don't make sense for Go
    if [ "$USE_VALGRIND" = true ] || [ "$USE_GDB" = true ]; then
        print_warning "Valgrind and GDB are not supported for Go CLI"
        print_info "Running without debugger..."
    fi

    print_info "Running Go CLI..."
    if [ ${#EXTRA_ARGS[@]} -gt 0 ]; then
        exec "$CLI_EXECUTABLE" "${EXTRA_ARGS[@]}"
    else
        exec "$CLI_EXECUTABLE"
    fi
    exit 0
fi

# Determine executable path for C++ targets
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
    if [ ${#EXTRA_ARGS[@]} -gt 0 ]; then
        valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes "$EXECUTABLE" "${EXTRA_ARGS[@]}"
    else
        valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes "$EXECUTABLE"
    fi
elif [ "$USE_GDB" = true ]; then
    if ! require_command gdb "  macOS:  brew install gdb
  Ubuntu: sudo apt-get install gdb"; then
        exit 1
    fi
    print_info "Running with gdb..."
    if [ ${#EXTRA_ARGS[@]} -gt 0 ]; then
        gdb --args "$EXECUTABLE" "${EXTRA_ARGS[@]}"
    else
        gdb "$EXECUTABLE"
    fi
else
    print_info "Running $TARGET..."
    if [ ${#EXTRA_ARGS[@]} -gt 0 ]; then
        "$EXECUTABLE" "${EXTRA_ARGS[@]}"
    else
        "$EXECUTABLE"
    fi
fi
