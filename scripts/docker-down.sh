#!/usr/bin/env bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

DOCKER_DIR="$ROOT_DIR/docker"
cd "$DOCKER_DIR"

CLEAN=false
REMOVE_IMAGES=false

show_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --clean, -c         Remove data volumes"
    echo "  --remove-images     Remove Docker images (forces rebuild next time)"
    echo "  --help, -h          Show this help"
    echo ""
    echo "Examples:"
    echo "  $0                      # Stop cluster, keep data and images"
    echo "  $0 --clean              # Stop cluster and remove data volumes"
    echo "  $0 --remove-images      # Stop cluster and remove images"
    echo "  $0 --clean --remove-images  # Full cleanup"
}

# Parse arguments
for arg in "$@"; do
    case "$arg" in
        --clean|-c)
            CLEAN=true
            ;;
        --remove-images)
            REMOVE_IMAGES=true
            ;;
        --help|-h)
            show_usage
            exit 0
            ;;
    esac
done

print_info "Stopping KV Store cluster..."
docker-compose down

if [ "$CLEAN" = true ]; then
    print_warning "Removing data volumes..."
    docker-compose down -v
fi

if [ "$REMOVE_IMAGES" = true ]; then
    print_warning "Removing Docker images..."
    docker-compose down --rmi local
    docker rmi docker-kv-node-1 docker-kv-node-2 docker-kv-node-3 2>/dev/null || true
fi

echo ""
if [ "$CLEAN" = true ] && [ "$REMOVE_IMAGES" = true ]; then
    print_success "Cluster stopped, data and images removed"
elif [ "$CLEAN" = true ]; then
    print_success "Cluster stopped and data cleaned"
elif [ "$REMOVE_IMAGES" = true ]; then
    print_success "Cluster stopped and images removed"
else
    print_success "Cluster stopped (data and images preserved)"
    echo ""
    print_info "Options:"
    echo "  --clean            Remove data volumes"
    echo "  --remove-images    Remove images (forces rebuild)"
fi
