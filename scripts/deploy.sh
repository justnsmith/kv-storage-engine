#!/usr/bin/env bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

TERRAFORM_DIR="$ROOT_DIR/terraform"

show_usage() {
    echo "Usage: $0 [COMMAND] [OPTIONS]"
    echo ""
    echo "Commands:"
    echo "  init         Initialize Terraform"
    echo "  plan         Show deployment plan"
    echo "  apply        Deploy infrastructure"
    echo "  destroy      Destroy infrastructure"
    echo "  output       Show outputs"
    echo "  ssh          SSH to a node"
    echo "  logs         View logs from a node"
    echo "  status       Check cluster status"
    echo ""
    echo "SSH Options:"
    echo "  leader       SSH to leader node"
    echo "  follower1    SSH to follower 1"
    echo "  follower2    SSH to follower 2"
    echo ""
    echo "Examples:"
    echo "  $0 init               # Initialize Terraform"
    echo "  $0 plan               # Preview changes"
    echo "  $0 apply              # Deploy cluster"
    echo "  $0 ssh leader         # SSH to leader"
    echo "  $0 logs follower1     # View follower 1 logs"
    echo "  $0 status             # Check all nodes"
    echo "  $0 destroy            # Tear down cluster"
}

require_terraform() {
    if ! command -v terraform &> /dev/null; then
        print_error "Terraform not found. Install it first:"
        echo "  macOS:  brew install terraform"
        echo "  Linux:  https://developer.hashicorp.com/terraform/install"
        exit 1
    fi
}

cd "$TERRAFORM_DIR"

COMMAND="${1:-help}"
shift || true

case "$COMMAND" in
    init)
        require_terraform
        print_header "Initializing Terraform"
        terraform init
        ;;

    plan)
        require_terraform
        print_header "Planning Deployment"
        terraform plan
        ;;

    apply)
        require_terraform
        print_header "Deploying KV Store Cluster to AWS"

        if [ ! -f "terraform.tfvars" ]; then
            print_error "terraform.tfvars not found!"
            echo ""
            echo "Create it from the example:"
            echo "  cp terraform.tfvars.example terraform.tfvars"
            echo "  # Edit terraform.tfvars with your settings"
            exit 1
        fi

        print_warning "This will create AWS resources (costs money!)"
        read -p "Continue? (yes/no): " confirm

        if [ "$confirm" != "yes" ]; then
            print_info "Deployment cancelled"
            exit 0
        fi

        terraform apply

        if [ $? -eq 0 ]; then
            echo ""
            print_success "Cluster deployed successfully!"
            echo ""
            print_info "Waiting for instances to initialize (60s)..."
            sleep 60
            echo ""
            print_info "Cluster endpoints:"
            terraform output -json | jq -r '.leader_endpoint.value'
            echo ""
            print_info "SSH commands:"
            terraform output -json | jq -r '.ssh_commands.value | to_entries[] | "  \(.key): \(.value)"'
            echo ""
            print_info "Test commands:"
            terraform output -json | jq -r '.test_commands.value | to_entries[] | "  \(.key): \(.value)"'
        fi
        ;;

    destroy)
        require_terraform
        print_header "Destroying KV Store Cluster"

        print_warning "This will DELETE all AWS resources and data!"
        read -p "Are you sure? (yes/no): " confirm

        if [ "$confirm" != "yes" ]; then
            print_info "Destruction cancelled"
            exit 0
        fi

        terraform destroy
        ;;

    output)
        require_terraform
        terraform output "$@"
        ;;

    ssh)
        require_terraform
        NODE="${1:-leader}"

        case "$NODE" in
            leader)
                IP=$(terraform output -raw leader_public_ip)
                ;;
            follower1)
                IP=$(terraform output -raw follower1_public_ip)
                ;;
            follower2)
                IP=$(terraform output -raw follower2_public_ip)
                ;;
            *)
                print_error "Unknown node: $NODE"
                echo "Use: leader, follower1, or follower2"
                exit 1
                ;;
        esac

        print_info "Connecting to $NODE ($IP)..."
        ssh -i ~/.ssh/kv-store ubuntu@$IP
        ;;

    logs)
        require_terraform
        NODE="${1:-leader}"

        case "$NODE" in
            leader)
                IP=$(terraform output -raw leader_public_ip)
                ;;
            follower1)
                IP=$(terraform output -raw follower1_public_ip)
                ;;
            follower2)
                IP=$(terraform output -raw follower2_public_ip)
                ;;
            *)
                print_error "Unknown node: $NODE"
                echo "Use: leader, follower1, or follower2"
                exit 1
                ;;
        esac

        print_info "Viewing logs from $NODE ($IP)..."
        ssh -i ~/.ssh/kv-store ubuntu@$IP "docker logs -f kv-store"
        ;;

    status)
        require_terraform
        print_header "Cluster Status"

        LEADER_IP=$(terraform output -raw leader_public_ip)
        FOLLOWER1_IP=$(terraform output -raw follower1_public_ip)
        FOLLOWER2_IP=$(terraform output -raw follower2_public_ip)

        echo "Leader ($LEADER_IP):"
        ssh -i ~/.ssh/kv-store ubuntu@$LEADER_IP "docker ps && echo '' && docker logs kv-store 2>&1 | tail -10" || echo "  ✗ Failed to connect"

        echo ""
        echo "Follower 1 ($FOLLOWER1_IP):"
        ssh -i ~/.ssh/kv-store ubuntu@$FOLLOWER1_IP "docker ps && echo '' && docker logs kv-store 2>&1 | tail -10" || echo "  ✗ Failed to connect"

        echo ""
        echo "Follower 2 ($FOLLOWER2_IP):"
        ssh -i ~/.ssh/kv-store ubuntu@$FOLLOWER2_IP "docker ps && echo '' && docker logs kv-store 2>&1 | tail -10" || echo "  ✗ Failed to connect"
        ;;

    help|--help|-h)
        show_usage
        ;;

    *)
        print_error "Unknown command: $COMMAND"
        echo ""
        show_usage
        exit 1
        ;;
esac
