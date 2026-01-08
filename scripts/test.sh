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
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --help, -h       Show this help message"
    echo "  --build          Build before testing"
    echo "  --verbose, -v    Verbose test output"
    echo "  --filter PATTERN Run only tests matching pattern"
    echo "  --valgrind       Run tests with valgrind"
    echo "  --coverage       Generate coverage report (requires gcov)"
    echo ""
    echo "Examples:"
    echo "  $0                           # Run all tests"
    echo "  $0 --build                   # Build and run tests"
    echo "  $0 --filter memtable         # Run only memtable tests"
    echo "  $0 --valgrind                # Run with memory checking"
}

# Parse arguments
BUILD_FIRST=false
VERBOSE=false
FILTER=""
USE_VALGRIND=false
GENERATE_COVERAGE=false

for arg in "$@"; do
    case "$arg" in
        --help|-h)
            show_usage
            exit 0
            ;;
        --build)
            BUILD_FIRST=true
            ;;
        --verbose|-v)
            VERBOSE=true
            ;;
        --filter)
            shift
            FILTER="$1"
            shift
            ;;
        --valgrind)
            USE_VALGRIND=true
            ;;
        --coverage)
            GENERATE_COVERAGE=true
            ;;
    esac
done

print_header "KV Storage Test Suite"

# Build if requested
if [ "$BUILD_FIRST" = true ]; then
    print_info "Building tests..."
    "$ROOT_DIR/scripts/build.sh" engine
    echo ""
fi

# Check if test executable exists
TEST_EXECUTABLE="$BUILD_DIR/engine/kv_engine_tests"
if [ ! -f "$TEST_EXECUTABLE" ]; then
    print_error "Test executable not found: $TEST_EXECUTABLE"
    echo "Run with --build to build first"
    exit 1
fi

cd "$ROOT_DIR/engine"

# Run tests
CTEST_ARGS="--output-on-failure"
if [ "$VERBOSE" = true ]; then
    CTEST_ARGS="$CTEST_ARGS --verbose"
fi

if [ -n "$FILTER" ]; then
    CTEST_ARGS="$CTEST_ARGS -R $FILTER"
fi

if [ "$USE_VALGRIND" = true ]; then
    if ! command -v valgrind &> /dev/null; then
        print_error "valgrind not found"
        exit 1
    fi
    print_info "Running tests with valgrind..."
    valgrind --leak-check=full --error-exitcode=1 "$TEST_EXECUTABLE"
    TEST_RESULT=$?
else
    print_info "Running tests..."
    (cd "$BUILD_DIR/engine" && ctest $CTEST_ARGS)
    TEST_RESULT=$?
fi

echo ""

if [ $TEST_RESULT -eq 0 ]; then
    print_success "All tests passed!"
else
    print_error "Some tests failed"
    exit 1
fi

# Generate coverage if requested
if [ "$GENERATE_COVERAGE" = true ]; then
    if ! command -v gcov &> /dev/null; then
        print_error "gcov not found. Install it for coverage reports."
        exit 1
    fi

    echo ""
    print_info "Generating coverage report..."

    cd "$BUILD_DIR/engine"
    find . -name "*.gcda" -exec gcov {} \; > /dev/null 2>&1 || true

    print_success "Coverage files generated in $BUILD_DIR/engine"
    echo "View .gcov files for detailed coverage information"
fi
