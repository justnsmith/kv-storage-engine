#!/usr/bin/env bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

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
    echo "  --docker             Include Docker build and basic connectivity test"
    echo ""
    echo "Examples:"
    echo "  $0                       # Full CI pipeline"
    echo "  $0 --fast                # Quick build and test"
    echo "  $0 --skip-tests          # Build and check only"
    echo "  $0 --fix-format          # Run CI and fix formatting"
    echo "  $0 --build-type Debug    # CI with debug build"
    echo "  $0 --docker              # Include Docker build/test"
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
TEST_DOCKER=false

while [ $# -gt 0 ]; do
    case "$1" in
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
            ;;
        --target)
            shift
            TARGET="$1"
            ;;
        --fast)
            FAST_MODE=true
            SKIP_CLEAN=true
            SKIP_CHECK=true
            SKIP_FORMAT=true
            ;;
        --docker)
            TEST_DOCKER=true
            ;;
        *)
            print_error "Unknown option: $1"
            show_usage
            exit 1
            ;;
    esac
    shift
done

# Normalize build type
BUILD_TYPE=$(normalize_build_type "$BUILD_TYPE")

# Track results
CLEAN_STATUS="⊝ skipped"
BUILD_STATUS="✗ failed"
TEST_STATUS="⊝ skipped"
CHECK_STATUS="⊝ skipped"
FORMAT_STATUS="⊝ skipped"
DOCKER_STATUS="⊝ skipped"
OVERALL_STATUS=0

START_TIME=$(start_timer)

print_fancy_header "KV Storage - Continuous Integration"
echo "Target:      $TARGET"
echo "Build Type:  $BUILD_TYPE"
if [ "$FAST_MODE" = true ]; then
    echo "Mode:        Fast (build + test only)"
fi
if [ "$TEST_DOCKER" = true ]; then
    echo "Docker:      Enabled"
fi
echo ""

# Step 1: Clean
if [ "$SKIP_CLEAN" = false ]; then
    print_fancy_header "Step 1: Clean"
    print_step "Cleaning build artifacts..."
    if "$SCRIPT_DIR/clean.sh" --build; then
        CLEAN_STATUS="✓ passed"
        print_success "Clean complete"
    else
        CLEAN_STATUS="✗ failed"
        print_error "Clean failed"
        OVERALL_STATUS=1
    fi
else
    print_fancy_header "Step 1: Clean (skipped)"
fi

# Step 2: Build
print_fancy_header "Step 2: Build"
print_step "Building $TARGET ($BUILD_TYPE)..."
if "$SCRIPT_DIR/build.sh" "$TARGET" "$BUILD_TYPE"; then
    BUILD_STATUS="✓ passed"
    print_success "Build complete"
else
    BUILD_STATUS="✗ failed"
    print_error "Build failed"
    OVERALL_STATUS=1

    # Print summary and exit early on build failure
    print_fancy_header "CI SUMMARY"
    echo ""
    echo "Pipeline Results:"
    echo "  Clean:    $CLEAN_STATUS"
    echo "  Build:    $BUILD_STATUS"
    echo ""
    print_error "CI pipeline failed at build stage"
    exit 1
fi

# Step 3: Tests
if [ "$SKIP_TESTS" = false ]; then
    print_fancy_header "Step 3: Tests"
    print_step "Running test suite..."
    if "$SCRIPT_DIR/test.sh"; then
        TEST_STATUS="✓ passed"
        print_success "All tests passed"
    else
        TEST_STATUS="✗ failed"
        print_error "Tests failed"
        OVERALL_STATUS=1
    fi
else
    print_fancy_header "Step 3: Tests (skipped)"
fi

# Step 4: Static Analysis
if [ "$SKIP_CHECK" = false ]; then
    print_fancy_header "Step 4: Static Analysis"
    print_step "Running cppcheck..."
    if "$SCRIPT_DIR/check.sh" "$TARGET"; then
        CHECK_STATUS="✓ passed"
        print_success "Static analysis passed"
    else
        CHECK_STATUS="✗ failed"
        print_error "Static analysis found issues"
        OVERALL_STATUS=1
    fi
else
    print_fancy_header "Step 4: Static Analysis (skipped)"
fi

# Step 5: Format Check
if [ "$SKIP_FORMAT" = false ]; then
    print_fancy_header "Step 5: Format Check"

    if [ "$FIX_FORMAT" = true ]; then
        print_step "Fixing format issues..."
        if "$SCRIPT_DIR/check.sh" "$TARGET" --fix-format; then
            FORMAT_STATUS="✓ fixed"
            print_success "Format issues fixed"
        else
            FORMAT_STATUS="✗ failed"
            print_error "Format fix failed"
            OVERALL_STATUS=1
        fi
    else
        print_step "Checking code formatting..."
        if "$SCRIPT_DIR/check.sh" "$TARGET" --format; then
            FORMAT_STATUS="✓ passed"
            print_success "Format check passed"
        else
            FORMAT_STATUS="✗ failed"
            print_error "Format check failed"
            OVERALL_STATUS=1
        fi
    fi
else
    print_fancy_header "Step 5: Format Check (skipped)"
fi

# Step 6: Docker Build & Test
if [ "$TEST_DOCKER" = true ]; then
    print_fancy_header "Step 6: Docker Build & Test"

    print_step "Building Docker images..."
    if "$SCRIPT_DIR/docker-build.sh" --no-cache > /tmp/docker-build.log 2>&1; then
        print_success "Docker images built"

        print_step "Starting cluster..."
        if "$SCRIPT_DIR/docker-up.sh" > /tmp/docker-up.log 2>&1; then
            print_success "Cluster started"

            # Wait for cluster to be ready
            print_step "Waiting for cluster to initialize..."
            sleep 5

            # Check if all containers are running
            RUNNING=$(docker-compose -f "$ROOT_DIR/docker/docker-compose.yml" ps --filter "status=running" --format "{{.Names}}" | wc -l)
            if [ "$RUNNING" -eq 3 ]; then
                print_success "All 3 nodes are running"

                # Check leader logs for successful connections
                CONNECTED=$(docker logs kv-leader 2>&1 | grep "Connection summary: 2/2 connected" | wc -l)
                if [ "$CONNECTED" -gt 0 ]; then
                    DOCKER_STATUS="✓ passed"
                    print_success "Nodes successfully connected"
                else
                    DOCKER_STATUS="✗ failed"
                    print_error "Nodes failed to connect"
                    echo "Leader logs:"
                    docker logs kv-leader 2>&1 | tail -20
                    OVERALL_STATUS=1
                fi
            else
                DOCKER_STATUS="✗ failed"
                print_error "Only $RUNNING/3 nodes running"
                OVERALL_STATUS=1
            fi

            # Cleanup
            print_step "Stopping cluster..."
            "$SCRIPT_DIR/docker-down.sh" > /dev/null 2>&1
            print_success "Cluster stopped"
        else
            DOCKER_STATUS="✗ failed"
            print_error "Failed to start cluster"
            cat /tmp/docker-up.log
            OVERALL_STATUS=1
        fi
    else
        DOCKER_STATUS="✗ failed"
        print_error "Docker build failed"
        cat /tmp/docker-build.log
        OVERALL_STATUS=1
    fi
else
    print_fancy_header "Step 6: Docker Build & Test (skipped)"
fi

# Summary
ELAPSED=$(get_elapsed_time $START_TIME)
DURATION=$(format_duration $ELAPSED)

print_fancy_header "CI SUMMARY"
echo ""
echo "Pipeline Results:"
echo "  Clean:    $CLEAN_STATUS"
echo "  Build:    $BUILD_STATUS"
echo "  Tests:    $TEST_STATUS"
echo "  Check:    $CHECK_STATUS"
echo "  Format:   $FORMAT_STATUS"
if [ "$TEST_DOCKER" = true ]; then
    echo "  Docker:   $DOCKER_STATUS"
fi
echo ""
echo "Duration: $DURATION"
echo ""

if [ $OVERALL_STATUS -eq 0 ]; then
    print_success "CI pipeline passed!"
    echo ""
    echo "Build artifacts:"
    if [ "$TARGET" = "all" ] || [ "$TARGET" = "engine" ]; then
        echo "  Engine: $(get_executable_path engine)"
    fi
    if [ "$TARGET" = "all" ] || [ "$TARGET" = "server" ]; then
        echo "  Server: $(get_executable_path server)"
    fi
else
    print_error "CI pipeline failed"
    exit 1
fi
