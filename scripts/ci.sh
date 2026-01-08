#!/usr/bin/env bash
set -e

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m'

print_header() {
    echo ""
    echo -e "${MAGENTA}╔════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${MAGENTA}║${NC}  ${CYAN}$1${NC}"
    echo -e "${MAGENTA}╚════════════════════════════════════════════════════════════════╝${NC}"
}

print_step() {
    echo -e "${BLUE}▶${NC} $1"
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

print_summary_header() {
    echo ""
    echo -e "${MAGENTA}╔════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${MAGENTA}║${NC}  ${CYAN}CI SUMMARY${NC}"
    echo -e "${MAGENTA}╚════════════════════════════════════════════════════════════════╝${NC}"
}

show_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Runs complete CI pipeline: clean, build, test, and check"
    echo ""
    echo "Options:"
    echo "  --help, -h           Show this help message"
    echo "  --skip-clean         Don't clean before building"
    echo "  --skip-tests         Skip running tests"
    echo "  --skip-check         Skip static analysis"
    echo "  --skip-format        Skip format checking"
    echo "  --fix-format         Fix formatting issues automatically"
    echo "  --build-type TYPE    Build type: Debug or Release (default: Release)"
    echo "  --fast               Skip clean, check, and format (build + test only)"
    echo "  --target TARGET      Build target: engine, server, or all (default: all)"
    echo ""
    echo "Examples:"
    echo "  $0                       # Full CI pipeline"
    echo "  $0 --fast                # Quick build and test"
    echo "  $0 --skip-tests          # Build and check only"
    echo "  $0 --fix-format          # Run CI and fix formatting"
    echo "  $0 --build-type Debug    # CI with debug build"
}

# Parse arguments
SKIP_CLEAN=false
SKIP_TESTS=false
SKIP_CHECK=false
SKIP_FORMAT=false
FIX_FORMAT=false
BUILD_TYPE="Release"
TARGET="all"
FAST_MODE=false

for arg in "$@"; do
    case "$arg" in
        --help|-h)
            show_usage
            exit 0
            ;;
        --skip-clean)
            SKIP_CLEAN=true
            ;;
        --skip-tests)
            SKIP_TESTS=true
            ;;
        --skip-check)
            SKIP_CHECK=true
            ;;
        --skip-format)
            SKIP_FORMAT=true
            ;;
        --fix-format)
            FIX_FORMAT=true
            ;;
        --build-type)
            shift
            BUILD_TYPE="$1"
            shift
            ;;
        --target)
            shift
            TARGET="$1"
            shift
            ;;
        --fast)
            FAST_MODE=true
            SKIP_CLEAN=true
            SKIP_CHECK=true
            SKIP_FORMAT=true
            ;;
        -*)
            print_error "Unknown option: $arg"
            show_usage
            exit 1
            ;;
    esac
done

# Track results
CLEAN_STATUS="⊝ skipped"
BUILD_STATUS="✗ failed"
TEST_STATUS="⊝ skipped"
CHECK_STATUS="⊝ skipped"
FORMAT_STATUS="⊝ skipped"
OVERALL_STATUS=0

START_TIME=$(date +%s)

print_header "KV Storage - Continuous Integration"
echo "Target:      $TARGET"
echo "Build Type:  $BUILD_TYPE"
if [ "$FAST_MODE" = true ]; then
    echo "Mode:        Fast (build + test only)"
fi
echo ""

# Step 1: Clean
if [ "$SKIP_CLEAN" = false ]; then
    print_header "Step 1: Clean"
    print_step "Cleaning build artifacts..."
    if "$ROOT_DIR/scripts/clean.sh" --build; then
        CLEAN_STATUS="✓ passed"
        print_success "Clean complete"
    else
        CLEAN_STATUS="✗ failed"
        print_error "Clean failed"
        OVERALL_STATUS=1
        # Non-fatal, continue anyway
    fi
else
    print_header "Step 1: Clean (skipped)"
fi

# Step 2: Build
print_header "Step 2: Build"
print_step "Building $TARGET ($BUILD_TYPE)..."
if "$ROOT_DIR/scripts/build.sh" "$TARGET" "$BUILD_TYPE"; then
    BUILD_STATUS="✓ passed"
    print_success "Build complete"
else
    BUILD_STATUS="✗ failed"
    print_error "Build failed"
    OVERALL_STATUS=1
    print_summary_header
    echo "Clean:   $CLEAN_STATUS"
    echo "Build:   $BUILD_STATUS"
    echo ""
    print_error "CI pipeline failed at build stage"
    exit 1
fi

# Step 3: Tests
if [ "$SKIP_TESTS" = false ]; then
    print_header "Step 3: Tests"
    print_step "Running test suite..."
    if "$ROOT_DIR/scripts/test.sh"; then
        TEST_STATUS="✓ passed"
        print_success "All tests passed"
    else
        TEST_STATUS="✗ failed"
        print_error "Tests failed"
        OVERALL_STATUS=1
    fi
else
    print_header "Step 3: Tests (skipped)"
fi

# Step 4: Static Analysis
if [ "$SKIP_CHECK" = false ]; then
    print_header "Step 4: Static Analysis"
    print_step "Running cppcheck..."
    if "$ROOT_DIR/scripts/check.sh" "$TARGET"; then
        CHECK_STATUS="✓ passed"
        print_success "Static analysis passed"
    else
        CHECK_STATUS="✗ failed"
        print_error "Static analysis found issues"
        OVERALL_STATUS=1
    fi
else
    print_header "Step 4: Static Analysis (skipped)"
fi

# Step 5: Format Check
if [ "$SKIP_FORMAT" = false ]; then
    print_header "Step 5: Format Check"

    if [ "$FIX_FORMAT" = true ]; then
        print_step "Fixing format issues..."
        if "$ROOT_DIR/scripts/check.sh" "$TARGET" --fix-format; then
            FORMAT_STATUS="✓ fixed"
            print_success "Format issues fixed"
        else
            FORMAT_STATUS="✗ failed"
            print_error "Format fix failed"
            OVERALL_STATUS=1
        fi
    else
        print_step "Checking code formatting..."
        if "$ROOT_DIR/scripts/check.sh" "$TARGET" --format; then
            FORMAT_STATUS="✓ passed"
            print_success "Format check passed"
        else
            FORMAT_STATUS="✗ failed"
            print_error "Format check failed"
            OVERALL_STATUS=1
        fi
    fi
else
    print_header "Step 5: Format Check (skipped)"
fi

# Summary
END_TIME=$(date +%s)
DURATION=$((END_TIME - START_TIME))

print_summary_header
echo ""
echo "Pipeline Results:"
echo "  Clean:    $CLEAN_STATUS"
echo "  Build:    $BUILD_STATUS"
echo "  Tests:    $TEST_STATUS"
echo "  Check:    $CHECK_STATUS"
echo "  Format:   $FORMAT_STATUS"
echo ""
echo "Duration: ${DURATION}s"
echo ""

if [ $OVERALL_STATUS -eq 0 ]; then
    print_success "CI pipeline passed! 🎉"
    echo ""
    echo "Build artifacts:"
    if [ "$TARGET" = "all" ] || [ "$TARGET" = "engine" ]; then
        echo "  Engine: $ROOT_DIR/build/engine/kv_engine"
    fi
    if [ "$TARGET" = "all" ] || [ "$TARGET" = "server" ]; then
        echo "  Server: $ROOT_DIR/build/server/kv_server"
    fi
else
    print_error "CI pipeline failed"
    exit 1
fi
