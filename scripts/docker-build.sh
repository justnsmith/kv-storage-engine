#!/usr/bin/env bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

DOCKER_DIR="$ROOT_DIR/docker"
NO_CACHE=""
PROGRESS=""

show_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --no-cache      Build without using cache (force clean rebuild)"
    echo "  --plain         Show plain build output (useful for debugging)"
    echo "  --help, -h      Show this help"
    echo ""
    echo "Examples:"
    echo "  $0                    # Normal build with cache"
    echo "  $0 --no-cache         # Force clean rebuild"
    echo "  $0 --no-cache --plain # Clean rebuild with detailed output"
}

# Parse arguments
for arg in "$@"; do
    case "$arg" in
        --no-cache)
            NO_CACHE="--no-cache"
            print_warning "Building without cache (clean rebuild)"
            ;;
        --plain)
            PROGRESS="--progress=plain"
            print_info "Using plain progress output"
            ;;
        --help|-h)
            show_usage
            exit 0
            ;;
    esac
done

print_header "Building KV Store Docker Images"
echo "Root dir:   $ROOT_DIR"
echo "Docker dir: $DOCKER_DIR"
echo ""

cd "$DOCKER_DIR"
TIMER=$(start_timer)

# Detect docker-compose command (V1 vs V2)
if command -v docker-compose &> /dev/null; then
    DOCKER_COMPOSE="docker-compose"
elif docker compose version &> /dev/null; then
    DOCKER_COMPOSE="docker compose"
else
    print_error "Neither 'docker-compose' nor 'docker compose' found"
    exit 1
fi

print_info "Using: $DOCKER_COMPOSE"

# Use docker-compose to build all services
if $DOCKER_COMPOSE build $NO_CACHE $PROGRESS; then
    ELAPSED=$(get_elapsed_time $TIMER)
    echo ""
    print_success "Docker images built successfully in $(format_duration $ELAPSED)!"
    echo ""
    echo "Images built:"
    docker images | grep -E "docker-(kv-node-[1-3]|REPOSITORY)" | head -4
    echo ""
    echo "Start cluster with:"
    echo "  ./scripts/run.sh docker:up"
else
    print_error "Docker build failed"
    exit 1
fi
