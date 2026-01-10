#!/usr/bin/env bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

show_usage() {
    echo "Usage: $0 [OPTIONS] [TARGET]"
    echo ""
    echo "Targets:"
    echo "  all          Check all code (default)"
    echo "  engine       Check only engine code"
    echo "  server       Check only server code"
    echo "  distributed  Check only distributed code"
    echo "  cli          Check only Go CLI code"
    echo "  cpp          Check only C++ code (engine + server + distributed)"
    echo "  go           Check only Go code (cli)"
    echo ""
    echo "Options:"
    echo "  --help, -h       Show this help message"
    echo "  --format         Run ONLY format checks (skip linters)"
    echo "  --fix-format     Fix formatting issues (skip linters)"
    echo "  --verbose        Verbose linter output"
    echo "  --xml            Output cppcheck results as XML"
}

# Parse arguments
TARGET="all"
CHECK_FORMAT=false
FIX_FORMAT=false
VERBOSE=false
XML_OUTPUT=false
FORMAT_ONLY=false

for arg in "$@"; do
    case "$arg" in
        --help|-h)
            show_usage
            exit 0
            ;;
        --format)
            CHECK_FORMAT=true
            FORMAT_ONLY=true
            ;;
        --fix-format)
            FIX_FORMAT=true
            FORMAT_ONLY=true
            ;;
        --verbose)
            VERBOSE=true
            ;;
        --xml)
            XML_OUTPUT=true
            ;;
        all|engine|server|distributed|cli|cpp|go)
            TARGET="$arg"
            ;;
        -*)
            print_error "Unknown option: $arg"
            show_usage
            exit 1
            ;;
    esac
done

CPPCHECK_RESULT=0
GOLANGCI_RESULT=0

# Determine what to check
CHECK_CPP=false
CHECK_GO=false

case "$TARGET" in
    all)
        CHECK_CPP=true
        CHECK_GO=true
        ;;
    cpp|engine|server|distributed)
        CHECK_CPP=true
        ;;
    go|cli)
        CHECK_GO=true
        ;;
esac

# Run cppcheck
if [ "$FORMAT_ONLY" = false ] && [ "$CHECK_CPP" = true ]; then
    if ! require_command cppcheck "Install cppcheck first."; then
        exit 1
    fi

    print_header "Static Analysis - cppcheck"

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

    [ "$VERBOSE" = true ] && CPPCHECK_ARGS+=(--verbose)
    [ "$XML_OUTPUT" = true ] && CPPCHECK_ARGS+=(--xml --xml-version=2)

    ENGINE_INCLUDES="-I$ROOT_DIR/engine/include"
    SERVER_INCLUDES="-I$ROOT_DIR/server/include"
    DISTRIBUTED_INCLUDES="-I$ROOT_DIR/distributed/include"

    case "$TARGET" in
        all|cpp)
            cppcheck "${CPPCHECK_ARGS[@]}" \
                $ENGINE_INCLUDES $SERVER_INCLUDES $DISTRIBUTED_INCLUDES \
                "$ROOT_DIR/engine/src" \
                "$ROOT_DIR/engine/tests" \
                "$ROOT_DIR/engine/benchmarks" \
                "$ROOT_DIR/server/src" \
                "$ROOT_DIR/distributed"
            ;;
        engine)
            cppcheck "${CPPCHECK_ARGS[@]}" \
                $ENGINE_INCLUDES \
                "$ROOT_DIR/engine/src" \
                "$ROOT_DIR/engine/tests" \
                "$ROOT_DIR/engine/benchmarks"
            ;;
        server)
            cppcheck "${CPPCHECK_ARGS[@]}" \
                $ENGINE_INCLUDES $SERVER_INCLUDES $DISTRIBUTED_INCLUDES \
                "$ROOT_DIR/server/src"
            ;;
        distributed)
            cppcheck "${CPPCHECK_ARGS[@]}" \
                $DISTRIBUTED_INCLUDES \
                "$ROOT_DIR/distributed"
            ;;
    esac

    CPPCHECK_RESULT=$?
fi

# Run golangci-lint
if [ "$FORMAT_ONLY" = false ] && [ "$CHECK_GO" = true ]; then
    print_header "Static Analysis - golangci-lint"

    if ! command -v golangci-lint &> /dev/null; then
        print_warning "golangci-lint not installed, skipping"
        GOLANGCI_RESULT=0
    else
        CLI_DIR="$ROOT_DIR/cli"

        if (cd "$CLI_DIR" && golangci-lint run ./...); then
            GOLANGCI_RESULT=0
        else
            GOLANGCI_RESULT=$?
        fi
    fi
fi

# Format checks for C++
if [ "$CHECK_CPP" = true ] && ([ "$FIX_FORMAT" = true ] || [ "$CHECK_FORMAT" = true ]); then
    print_header "Format Check - clang-format (C++)"

    require_command clang-format "Install clang-format first." || exit 1

    CPP_FILES=()
    case "$TARGET" in
        all|cpp)
            CPP_FILES+=($(find "$ROOT_DIR/engine" "$ROOT_DIR/server" "$ROOT_DIR/distributed" -name "*.cpp" -o -name "*.h"))
            ;;
        engine)
            CPP_FILES+=($(find "$ROOT_DIR/engine" -name "*.cpp" -o -name "*.h"))
            ;;
        server)
            CPP_FILES+=($(find "$ROOT_DIR/server" -name "*.cpp" -o -name "*.h"))
            ;;
        distributed)
            CPP_FILES+=($(find "$ROOT_DIR/distributed" -name "*.cpp" -o -name "*.h"))
            ;;
    esac

    if [ "$FIX_FORMAT" = true ]; then
        for file in "${CPP_FILES[@]}"; do
            clang-format -i "$file"
        done
    else
        for file in "${CPP_FILES[@]}"; do
            clang-format --dry-run -Werror "$file" || exit 1
        done
    fi
fi

# Format checks for Go
if [ "$CHECK_GO" = true ] && ([ "$FIX_FORMAT" = true ] || [ "$CHECK_FORMAT" = true ]); then
    print_header "Format Check - gofmt (Go)"

    command -v go &> /dev/null || exit 0

    CLI_DIR="$ROOT_DIR/cli"

    if [ "$FIX_FORMAT" = true ]; then
        (cd "$CLI_DIR" && gofmt -w .)
    else
        UNFORMATTED=$(cd "$CLI_DIR" && gofmt -l .)
        [ -z "$UNFORMATTED" ] || exit 1
    fi
fi

# Final result
if [ "$FORMAT_ONLY" = false ]; then
    if [ $CPPCHECK_RESULT -ne 0 ] || [ $GOLANGCI_RESULT -ne 0 ]; then
        exit 1
    fi

    print_success "All checks passed!"
fi
