#!/usr/bin/env bash
set -e

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

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

show_usage() {
    echo "Usage: $0 [OPTIONS] [TARGET]"
    echo ""
    echo "Targets:"
    echo "  all          Check all code (default)"
    echo "  engine       Check only engine code"
    echo "  server       Check only server code"
    echo ""
    echo "Options:"
    echo "  --help, -h       Show this help message"
    echo "  --format         Also run clang-format check"
    echo "  --fix-format     Fix formatting issues with clang-format"
    echo "  --verbose        Verbose cppcheck output"
    echo "  --xml            Output cppcheck results as XML"
    echo ""
    echo "Examples:"
    echo "  $0                    # Check all code"
    echo "  $0 engine             # Check only engine"
    echo "  $0 --format           # Check code and formatting"
    echo "  $0 --fix-format       # Fix formatting issues"
}

# Parse arguments
TARGET="all"
CHECK_FORMAT=false
FIX_FORMAT=false
VERBOSE=false
XML_OUTPUT=false

for arg in "$@"; do
    case "$arg" in
        --help|-h)
            show_usage
            exit 0
            ;;
        --format)
            CHECK_FORMAT=true
            ;;
        --fix-format)
            FIX_FORMAT=true
            ;;
        --verbose)
            VERBOSE=true
            ;;
        --xml)
            XML_OUTPUT=true
            ;;
        all|engine|server)
            TARGET="$arg"
            ;;
        -*)
            print_error "Unknown option: $arg"
            show_usage
            exit 1
            ;;
    esac
done

# Check if cppcheck is available
if ! command -v cppcheck &> /dev/null; then
    print_error "cppcheck not found. Install it first:"
    echo "  macOS:  brew install cppcheck"
    echo "  Ubuntu: sudo apt-get install cppcheck"
    exit 1
fi

print_header "Static Analysis - cppcheck"

# Base cppcheck arguments
CPPCHECK_ARGS=(
    --enable=all
    --inconclusive
    --std=c++20
    --suppress=missingIncludeSystem
    --suppress=checkersReport
    --suppress=functionConst
    --suppress=normalCheckLevelMaxBranches
    --inline-suppr
)

if [ "$VERBOSE" = true ]; then
    CPPCHECK_ARGS+=(--verbose)
fi

if [ "$XML_OUTPUT" = true ]; then
    CPPCHECK_ARGS+=(--xml --xml-version=2)
fi

# Determine what to check
ENGINE_INCLUDES="-I$ROOT_DIR/engine/include"
SERVER_INCLUDES="-I$ROOT_DIR/server/include"

case "$TARGET" in
    all)
        print_info "Checking all C++ files in engine and server..."
        cppcheck "${CPPCHECK_ARGS[@]}" \
            $ENGINE_INCLUDES $SERVER_INCLUDES \
            "$ROOT_DIR/engine/src" \
            "$ROOT_DIR/engine/tests" \
            "$ROOT_DIR/engine/benchmarks" \
            "$ROOT_DIR/server/src"
        ;;
    engine)
        print_info "Checking engine C++ files..."
        cppcheck "${CPPCHECK_ARGS[@]}" \
            $ENGINE_INCLUDES \
            "$ROOT_DIR/engine/src" \
            "$ROOT_DIR/engine/tests" \
            "$ROOT_DIR/engine/benchmarks"
        ;;
    server)
        print_info "Checking server C++ files..."
        cppcheck "${CPPCHECK_ARGS[@]}" \
            $ENGINE_INCLUDES $SERVER_INCLUDES \
            "$ROOT_DIR/server/src"
        ;;
    *)
        print_error "Unknown target: $TARGET"
        show_usage
        exit 1
        ;;
esac

CPPCHECK_RESULT=$?

echo ""
if [ $CPPCHECK_RESULT -eq 0 ]; then
    print_success "cppcheck analysis complete"
else
    print_error "cppcheck found issues"
fi

# Format checking/fixing
if [ "$FIX_FORMAT" = true ] || [ "$CHECK_FORMAT" = true ]; then
    echo ""
    print_header "Format Check - clang-format"

    if ! command -v clang-format &> /dev/null; then
        print_error "clang-format not found. Install it first:"
        echo "  macOS:  brew install clang-format"
        echo "  Ubuntu: sudo apt-get install clang-format"
        exit 1
    fi

    # Find all C++ files
    CPP_FILES=()
    case "$TARGET" in
        all)
            CPP_FILES+=($(find "$ROOT_DIR/engine" -name "*.cpp" -o -name "*.h"))
            CPP_FILES+=($(find "$ROOT_DIR/server" -name "*.cpp" -o -name "*.h"))
            ;;
        engine)
            CPP_FILES+=($(find "$ROOT_DIR/engine" -name "*.cpp" -o -name "*.h"))
            ;;
        server)
            CPP_FILES+=($(find "$ROOT_DIR/server" -name "*.cpp" -o -name "*.h"))
            ;;
    esac

    if [ "$FIX_FORMAT" = true ]; then
        print_info "Fixing formatting issues..."
        for file in "${CPP_FILES[@]}"; do
            clang-format -i "$file"
        done
        print_success "Formatting fixed for ${#CPP_FILES[@]} files"
    else
        print_info "Checking formatting..."
        FORMAT_ISSUES=0
        for file in "${CPP_FILES[@]}"; do
            if ! clang-format --dry-run -Werror "$file" 2>/dev/null; then
                echo "  Format issues: $file"
                FORMAT_ISSUES=$((FORMAT_ISSUES + 1))
            fi
        done

        echo ""
        if [ $FORMAT_ISSUES -eq 0 ]; then
            print_success "All files properly formatted"
        else
            print_error "$FORMAT_ISSUES files have formatting issues"
            echo "Run with --fix-format to automatically fix them"
            exit 1
        fi
    fi
fi

echo ""
if [ $CPPCHECK_RESULT -eq 0 ]; then
    print_success "All checks passed!"
else
    exit 1
fi
