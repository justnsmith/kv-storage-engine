#!/usr/bin/env bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

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

while [ $# -gt 0 ]; do
    case "$1" in
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
            ;;
        --valgrind)
            USE_VALGRIND=true
            ;;
        --coverage)
            GENERATE_COVERAGE=true
            ;;
        *)
            print_error "Unknown option: $1"
            show_usage
            exit 1
            ;;
    esac
    shift
done

print_header "KV Storage Test Suite"

# Build if requested
if [ "$BUILD_FIRST" = true ]; then
    print_info "Building tests..."
    "$SCRIPT_DIR/build.sh" engine
    echo ""
fi

# Check if test executable exists
TEST_EXECUTABLE=$(get_executable_path tests)
if ! check_executable "$TEST_EXECUTABLE" "test executable"; then
    echo "Run with --build to build first"
    exit 1
fi

# Change to engine directory
cd "$(get_working_dir tests)"

# Run tests
CTEST_ARGS="--output-on-failure"

if [ "$VERBOSE" = true ]; then
    CTEST_ARGS="$CTEST_ARGS --verbose"
fi

if [ -n "$FILTER" ]; then
    CTEST_ARGS="$CTEST_ARGS -R $FILTER"
fi

if [ "$USE_VALGRIND" = true ]; then
    if ! require_command valgrind "  macOS:  brew install valgrind
  Ubuntu: sudo apt-get install valgrind"; then
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
    if ! require_command gcov "  macOS:  brew install gcc
  Ubuntu: sudo apt-get install gcov"; then
        exit 1
    fi

    echo ""
    print_info "Generating coverage report..."
    cd "$BUILD_DIR/engine"
    find . -name "*.gcda" -exec gcov {} \; > /dev/null 2>&1 || true
    print_success "Coverage files generated in $BUILD_DIR/engine"
    echo "View .gcov files for detailed coverage information"
fi
