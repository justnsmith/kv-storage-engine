#!/usr/bin/env bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

show_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --help, -h       Show this help message"
    echo "  --all, -a        Clean build dir, engine data, and server data"
    echo "  --data, -d       Clean only engine and server data files"
    echo "  --build, -b      Clean only build directory (default)"
    echo "  --engine         Clean only engine data files"
    echo "  --server         Clean only server data files"
    echo "  --dry-run, -n    Show what would be deleted without deleting"
    echo ""
    echo "Examples:"
    echo "  $0                # Clean build directory"
    echo "  $0 --all          # Clean everything"
    echo "  $0 --data         # Clean data files only"
    echo "  $0 --dry-run      # Preview what will be cleaned"
}

# Default values
CLEAN_BUILD=false
CLEAN_ENGINE_DATA=false
CLEAN_SERVER_DATA=false
DRY_RUN=false

# Parse arguments
if [ $# -eq 0 ]; then
    CLEAN_BUILD=true
fi

for arg in "$@"; do
    case "$arg" in
        --help|-h)
            show_usage
            exit 0
            ;;
        --all|-a)
            CLEAN_BUILD=true
            CLEAN_ENGINE_DATA=true
            CLEAN_SERVER_DATA=true
            ;;
        --data|-d)
            CLEAN_ENGINE_DATA=true
            CLEAN_SERVER_DATA=true
            ;;
        --build|-b)
            CLEAN_BUILD=true
            ;;
        --engine)
            CLEAN_ENGINE_DATA=true
            ;;
        --server)
            CLEAN_SERVER_DATA=true
            ;;
        --dry-run|-n)
            DRY_RUN=true
            ;;
        *)
            echo "Unknown option: $arg"
            show_usage
            exit 1
            ;;
    esac
done

print_header "KV Storage Clean Script"

if [ "$DRY_RUN" = true ]; then
    print_warning "DRY RUN MODE - No files will be deleted"
    echo ""
fi

# Clean build directory
if [ "$CLEAN_BUILD" = true ]; then
    echo ""
    print_info "Cleaning build artifacts..."
    safe_remove "$BUILD_DIR" "build directory" "$DRY_RUN"
fi

# Clean engine data
if [ "$CLEAN_ENGINE_DATA" = true ]; then
    echo ""
    print_info "Cleaning engine data files..."
    safe_remove "$ROOT_DIR/engine/data" "engine data directory" "$DRY_RUN"
    safe_remove "$ROOT_DIR/engine/*.wal" "engine WAL files" "$DRY_RUN"
    safe_remove "$ROOT_DIR/engine/*.sst" "engine SSTable files" "$DRY_RUN"
    safe_remove "$ROOT_DIR/engine/*.log" "engine log files" "$DRY_RUN"

    # Find and clean any data directories in subdirectories
    if [ "$DRY_RUN" = false ]; then
        find "$ROOT_DIR/engine" -type d -name "data" -exec rm -rf {} + 2>/dev/null || true
        find "$ROOT_DIR/engine" -type f \( -name "*.wal" -o -name "*.sst" -o -name "*.log" \) -delete 2>/dev/null || true
    fi
fi

# Clean server data
if [ "$CLEAN_SERVER_DATA" = true ]; then
    echo ""
    print_info "Cleaning server data files..."
    safe_remove "$ROOT_DIR/server/data" "server data directory" "$DRY_RUN"
    safe_remove "$ROOT_DIR/server/*.log" "server log files" "$DRY_RUN"

    if [ "$DRY_RUN" = false ]; then
        find "$ROOT_DIR/server" -type d -name "data" -exec rm -rf {} + 2>/dev/null || true
        find "$ROOT_DIR/server" -type f -name "*.log" -delete 2>/dev/null || true
    fi
fi

echo ""
if [ "$DRY_RUN" = true ]; then
    print_warning "Dry run complete - no files were deleted"
    echo "Run without --dry-run to actually clean files"
else
    print_success "Clean complete!"
fi
