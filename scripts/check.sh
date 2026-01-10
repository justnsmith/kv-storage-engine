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
    echo "  cli          Check only Go CLI code"
    echo "  cpp          Check only C++ code (engine + server)"
    echo "  go           Check only Go code (cli)"
    echo ""
    echo "Options:"
    echo "  --help, -h       Show this help message"
    echo "  --format         Run ONLY format checks (skip linters)"
    echo "  --fix-format     Fix formatting issues (skip linters)"
    echo "  --verbose        Verbose linter output"
    echo "  --xml            Output cppcheck results as XML"
    echo ""
    echo "Examples:"
    echo "  $0                    # Check all code (C++ and Go)"
    echo "  $0 engine             # Check only engine with cppcheck"
    echo "  $0 cli                # Check only Go CLI"
    echo "  $0 --format           # Check formatting only"
    echo "  $0 --fix-format       # Fix formatting issues only"
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
        all|engine|server|cli|cpp|go)
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

# Determine what to check based on target
CHECK_CPP=false
CHECK_GO=false

case "$TARGET" in
    all)
        CHECK_CPP=true
        CHECK_GO=true
        ;;
    cpp|engine|server)
        CHECK_CPP=true
        ;;
    go|cli)
        CHECK_GO=true
        ;;
esac

# Run cppcheck unless we're in format-only mode
if [ "$FORMAT_ONLY" = false ] && [ "$CHECK_CPP" = true ]; then
    # Check if cppcheck is available
    if ! require_command cppcheck "  macOS:  brew install cppcheck
  Ubuntu: sudo apt-get install cppcheck"; then
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
        all|cpp)
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
    esac

    CPPCHECK_RESULT=$?

    echo ""
    if [ $CPPCHECK_RESULT -eq 0 ]; then
        print_success "cppcheck analysis complete"
    else
        print_error "cppcheck found issues"
    fi
fi

# Run golangci-lint unless we're in format-only mode
if [ "$FORMAT_ONLY" = false ] && [ "$CHECK_GO" = true ]; then
    if [ "$CHECK_CPP" = true ]; then
        echo ""
    fi

    print_header "Static Analysis - golangci-lint"

    # Check if golangci-lint is available
    if ! command -v golangci-lint &> /dev/null; then
        print_warning "golangci-lint is not installed"
        echo "Install it with:"
        echo "  macOS:  brew install golangci-lint"
        echo "  Linux:  curl -sSfL https://raw.githubusercontent.com/golangci/golangci-lint/master/install.sh | sh -s -- -b \$(go env GOPATH)/bin"
        echo ""
        print_info "Skipping Go lint checks"
        GOLANGCI_RESULT=0
    else
        CLI_DIR="$ROOT_DIR/cli"

        if [ ! -d "$CLI_DIR" ]; then
            print_error "CLI directory not found: $CLI_DIR"
            GOLANGCI_RESULT=1
        else
            print_info "Checking Go files in cli/..."

            GOLANGCI_ARGS=()
            if [ "$VERBOSE" = true ]; then
                GOLANGCI_ARGS+=(--verbose)
            fi

            if (cd "$CLI_DIR" && golangci-lint run "${GOLANGCI_ARGS[@]}" ./...); then
                GOLANGCI_RESULT=0
                echo ""
                print_success "golangci-lint analysis complete"
            else
                GOLANGCI_RESULT=$?
                echo ""
                print_error "golangci-lint found issues"
            fi
        fi
    fi
fi

# Format checking/fixing for C++
if [ "$CHECK_CPP" = true ] && ([ "$FIX_FORMAT" = true ] || [ "$CHECK_FORMAT" = true ]); then
    if [ "$FORMAT_ONLY" = false ]; then
        echo ""
    fi
    print_header "Format Check - clang-format (C++)"

    if ! require_command clang-format "  macOS:  brew install clang-format
  Ubuntu: sudo apt-get install clang-format"; then
        exit 1
    fi

    # Find all C++ files
    CPP_FILES=()
    case "$TARGET" in
        all|cpp)
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
        print_info "Fixing C++ formatting issues..."
        for file in "${CPP_FILES[@]}"; do
            clang-format -i "$file"
        done
        print_success "Formatting fixed for ${#CPP_FILES[@]} C++ files"
    else
        print_info "Checking C++ formatting..."
        FORMAT_ISSUES=0
        for file in "${CPP_FILES[@]}"; do
            if ! clang-format --dry-run -Werror "$file" 2>/dev/null; then
                echo "  Format issues: $file"
                FORMAT_ISSUES=$((FORMAT_ISSUES + 1))
            fi
        done

        echo ""
        if [ $FORMAT_ISSUES -eq 0 ]; then
            print_success "All C++ files properly formatted"
        else
            print_error "$FORMAT_ISSUES C++ files have formatting issues"
            echo "Run with --fix-format to automatically fix them"
            exit 1
        fi
    fi
fi

# Format checking/fixing for Go
if [ "$CHECK_GO" = true ] && ([ "$FIX_FORMAT" = true ] || [ "$CHECK_FORMAT" = true ]); then
    if [ "$CHECK_CPP" = true ] || [ "$FORMAT_ONLY" = false ]; then
        echo ""
    fi
    print_header "Format Check - gofmt (Go)"

    # Check if Go is installed
    if ! command -v go &> /dev/null; then
        print_warning "Go is not installed, skipping Go format checks"
    else
        CLI_DIR="$ROOT_DIR/cli"

        if [ ! -d "$CLI_DIR" ]; then
            print_error "CLI directory not found: $CLI_DIR"
        else
            if [ "$FIX_FORMAT" = true ]; then
                print_info "Fixing Go formatting issues..."
                if (cd "$CLI_DIR" && gofmt -w .); then
                    print_success "Go formatting fixed"
                else
                    print_error "Failed to fix Go formatting"
                    exit 1
                fi
            else
                print_info "Checking Go formatting..."
                UNFORMATTED=$(cd "$CLI_DIR" && gofmt -l .)

                if [ -z "$UNFORMATTED" ]; then
                    print_success "All Go files properly formatted"
                else
                    print_error "Go files need formatting:"
                    echo "$UNFORMATTED" | while read -r file; do
                        echo "  Format issues: $file"
                    done
                    echo ""
                    echo "Run with --fix-format to automatically fix them"
                    exit 1
                fi
            fi
        fi
    fi
fi

# Exit with appropriate result
if [ "$FORMAT_ONLY" = false ]; then
    echo ""
    OVERALL_RESULT=0

    if [ $CPPCHECK_RESULT -ne 0 ]; then
        OVERALL_RESULT=1
    fi

    if [ $GOLANGCI_RESULT -ne 0 ]; then
        OVERALL_RESULT=1
    fi

    if [ $OVERALL_RESULT -eq 0 ]; then
        print_success "All checks passed!"
    else
        exit 1
    fi
fi
