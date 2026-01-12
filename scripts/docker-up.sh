#!/usr/bin/env bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

DOCKER_DIR="$ROOT_DIR/docker"
cd "$DOCKER_DIR"

# Detect docker-compose command (V1 vs V2)
if command -v docker-compose &> /dev/null; then
    DOCKER_COMPOSE="docker-compose"
elif docker compose version &> /dev/null; then
    DOCKER_COMPOSE="docker compose"
else
    print_error "Neither 'docker-compose' nor 'docker compose' found"
    exit 1
fi

print_header "Starting KV Store Cluster"

if $DOCKER_COMPOSE up -d; then
    echo ""
    print_success "Cluster started!"
    echo ""
    print_step "Cluster status:"
    $DOCKER_COMPOSE ps
    echo ""
    print_info "Node endpoints:"
    echo "  Leader:     http://localhost:9000"
    echo "  Follower 1: http://localhost:9001"
    echo "  Follower 2: http://localhost:9002"
    echo ""
    print_info "Useful commands:"
    echo "  View logs:  ./scripts/run.sh docker:logs"
    echo "  Stop:       ./scripts/run.sh docker:down"
else
    print_error "Failed to start cluster"
    exit 1
fi
