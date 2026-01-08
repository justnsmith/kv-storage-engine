#!/usr/bin/env bash
# common.sh - Shared functions and utilities for KV Storage scripts

set -e

# Get the root directory of the project
get_root_dir() {
    local script_dir="$(cd "$(dirname "${BASH_SOURCE[1]}")" && pwd)"
    echo "$(cd "$script_dir/.." && pwd)"
}

# Common directory variables
ROOT_DIR="${ROOT_DIR:-$(get_root_dir)}"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build}"

# ANSI color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

print_header() {
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BLUE}  $1${NC}"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
}

print_fancy_header() {
    echo ""
    echo -e "${MAGENTA}╔════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${MAGENTA}║${NC}  ${CYAN}$1${NC}"
    echo -e "${MAGENTA}╚════════════════════════════════════════════════════════════════╝${NC}"
}

print_success() {
    echo -e "${GREEN}✓${NC} $1"
}

print_error() {
    echo -e "${RED}✗${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}⚠${NC} $1"
}

print_info() {
    echo -e "${YELLOW}→${NC} $1"
}

print_step() {
    echo -e "${BLUE}▶${NC} $1"
}

# Detect number of CPU cores for parallel builds
get_cpu_count() {
    nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4
}

# Check if a command exists
command_exists() {
    command -v "$1" &> /dev/null
}

# Check if a command exists, print error and exit if not
require_command() {
    local cmd=$1
    local install_hint=$2

    if ! command_exists "$cmd"; then
        print_error "$cmd not found. Install it first:"
        if [ -n "$install_hint" ]; then
            echo "$install_hint"
        fi
        return 1
    fi
    return 0
}

# Configure and build a CMake project
cmake_configure_and_build() {
    local name=$1
    local src=$2
    local build_type=${3:-Release}
    local out="$BUILD_DIR/$name"
    local jobs=${4:-$(get_cpu_count)}

    print_info "Configuring $name ($build_type)"
    if cmake -S "$src" -B "$out" \
        -DCMAKE_BUILD_TYPE="$build_type" \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON; then
        print_success "Configuration complete for $name"
    else
        print_error "Configuration failed for $name"
        return 1
    fi

    print_info "Building $name with $jobs parallel jobs"
    if cmake --build "$out" -j"$jobs"; then
        print_success "Build complete for $name"
    else
        print_error "Build failed for $name"
        return 1
    fi
}

# Safely remove files/directories
safe_remove() {
    local path=$1
    local description=$2
    local dry_run=${3:-false}

    if [ ! -e "$path" ]; then
        print_info "$description does not exist, skipping"
        return 0
    fi

    local size=""
    if [ -d "$path" ]; then
        size=$(du -sh "$path" 2>/dev/null | cut -f1 || echo "unknown size")
    elif [ -f "$path" ]; then
        size=$(du -h "$path" 2>/dev/null | cut -f1 || echo "unknown size")
    fi

    if [ "$dry_run" = true ]; then
        print_info "Would remove: $path ($size)"
    else
        print_info "Removing $description ($size)"
        rm -rf "$path"
        print_success "Removed $description"
    fi
}

# Get executable path for a given target
get_executable_path() {
    local target=$1
    local benchmark_name=${2:-}

    case "$target" in
        engine)
            echo "$BUILD_DIR/engine/kv_engine"
            ;;
        server)
            echo "$BUILD_DIR/server/kv_server"
            ;;
        tests)
            echo "$BUILD_DIR/engine/kv_engine_tests"
            ;;
        benchmark)
            if [ -z "$benchmark_name" ]; then
                echo ""
            else
                echo "$BUILD_DIR/engine/benchmark_$benchmark_name"
            fi
            ;;
        *)
            echo ""
            ;;
    esac
}

# Get working directory for a given target
get_working_dir() {
    local target=$1

    case "$target" in
        engine|tests|benchmark)
            echo "$ROOT_DIR/engine"
            ;;
        server)
            echo "$ROOT_DIR/server"
            ;;
        *)
            echo "$ROOT_DIR"
            ;;
    esac
}

# Check if an executable exists
check_executable() {
    local executable=$1
    local target_name=${2:-executable}

    if [ ! -f "$executable" ]; then
        print_error "$target_name not found: $executable"
        echo "Build it first with: ./scripts/build.sh"
        return 1
    fi
    return 0
}

# Check if array contains a value
array_contains() {
    local seeking=$1
    shift
    local array=("$@")

    for element in "${array[@]}"; do
        if [ "$element" = "$seeking" ]; then
            return 0
        fi
    done
    return 1
}

# Parse common build types
normalize_build_type() {
    local build_type=$1
    local lower=$(echo "$build_type" | tr '[:upper:]' '[:lower:]')

    case "$lower" in
        debug|d)
            echo "Debug"
            ;;
        release|rel|r)
            echo "Release"
            ;;
        relwithdebinfo|relwithdeb|rwdi)
            echo "RelWithDebInfo"
            ;;
        minsizerel|minsize|ms)
            echo "MinSizeRel"
            ;;
        *)
            echo "$build_type"
            ;;
    esac
}

# Start a timer (stores start time in variable)
start_timer() {
    echo $(date +%s)
}

# Get elapsed time since timer start
get_elapsed_time() {
    local start_time=$1
    local end_time=$(date +%s)
    echo $((end_time - start_time))
}

# Format seconds into human-readable duration
format_duration() {
    local total_seconds=$1
    local minutes=$((total_seconds / 60))
    local seconds=$((total_seconds % 60))

    if [ $minutes -gt 0 ]; then
        echo "${minutes}m ${seconds}s"
    else
        echo "${seconds}s"
    fi
}

export ROOT_DIR BUILD_DIR
export RED GREEN YELLOW BLUE MAGENTA CYAN NC
