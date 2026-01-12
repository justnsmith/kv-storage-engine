#!/usr/bin/env bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

DOCKER_DIR="$ROOT_DIR/docker"

cd "$DOCKER_DIR"

show_usage() {
    echo "Usage: $0 [NODE] [OPTIONS]"
    echo ""
    echo "Nodes:"
    echo "  1, leader       Follow node 1 (leader)"
    echo "  2, follower1    Follow node 2 (follower)"
    echo "  3, follower2    Follow node 3 (follower)"
    echo "  all (default)   Follow all nodes"
    echo ""
    echo "Options:"
    echo "  -n NUM          Show last NUM lines"
    echo "  --help, -h      Show this help"
    echo ""
    echo "Examples:"
    echo "  $0              # Follow all nodes"
    echo "  $0 1            # Follow leader"
    echo "  $0 2 -n 100     # Show last 100 lines from follower 1"
}

NODE="${1:-all}"
FOLLOW="-f"
LINES=""

# Parse additional arguments
shift || true
while [ $# -gt 0 ]; do
    case "$1" in
        -n)
            LINES="--tail=$2"
            shift 2
            ;;
        --help|-h)
            show_usage
            exit 0
            ;;
        *)
            shift
            ;;
    esac
done

case "$NODE" in
    1|leader)
        print_info "Following logs for node 1 (leader)..."
        docker-compose logs $FOLLOW $LINES kv-node-1
        ;;
    2|follower1)
        print_info "Following logs for node 2 (follower 1)..."
        docker-compose logs $FOLLOW $LINES kv-node-2
        ;;
    3|follower2)
        print_info "Following logs for node 3 (follower 2)..."
        docker-compose logs $FOLLOW $LINES kv-node-3
        ;;
    all)
        print_info "Following logs for all nodes (Ctrl+C to exit)..."
        docker-compose logs $FOLLOW $LINES
        ;;
    *)
        print_error "Unknown node: $NODE"
        echo ""
        show_usage
        exit 1
        ;;
esac
