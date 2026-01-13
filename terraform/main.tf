terraform {
  required_version = ">= 1.0"

  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 5.0"
    }
  }
}

provider "aws" {
  region = var.aws_region
}

# VPC and Networking
resource "aws_vpc" "kv_store_vpc" {
  cidr_block           = "10.0.0.0/16"
  enable_dns_hostnames = true
  enable_dns_support   = true

  tags = {
    Name        = "kv-store-vpc"
    Project     = "kv-store"
    Environment = var.environment
  }
}

resource "aws_subnet" "kv_store_subnet" {
  vpc_id                  = aws_vpc.kv_store_vpc.id
  cidr_block              = "10.0.1.0/24"
  availability_zone       = data.aws_availability_zones.available.names[0]
  map_public_ip_on_launch = true

  tags = {
    Name        = "kv-store-subnet"
    Project     = "kv-store"
    Environment = var.environment
  }
}

resource "aws_internet_gateway" "kv_store_igw" {
  vpc_id = aws_vpc.kv_store_vpc.id

  tags = {
    Name        = "kv-store-igw"
    Project     = "kv-store"
    Environment = var.environment
  }
}

resource "aws_route_table" "kv_store_rt" {
  vpc_id = aws_vpc.kv_store_vpc.id

  route {
    cidr_block = "0.0.0.0/0"
    gateway_id = aws_internet_gateway.kv_store_igw.id
  }

  tags = {
    Name        = "kv-store-rt"
    Project     = "kv-store"
    Environment = var.environment
  }
}

resource "aws_route_table_association" "kv_store_rta" {
  subnet_id      = aws_subnet.kv_store_subnet.id
  route_table_id = aws_route_table.kv_store_rt.id
}

# Security Groups
resource "aws_security_group" "kv_store_sg" {
  name        = "kv-store-sg"
  description = "Security group for KV Store cluster"
  vpc_id      = aws_vpc.kv_store_vpc.id

  # SSH access
  ingress {
    from_port   = 22
    to_port     = 22
    protocol    = "tcp"
    cidr_blocks = var.allowed_ssh_cidr
    description = "SSH access"
  }

  # Client ports (9000-9002)
  ingress {
    from_port   = 9000
    to_port     = 9002
    protocol    = "tcp"
    cidr_blocks = var.allowed_client_cidr
    description = "KV Store client access"
  }

  # Replication ports (9100-9102) - only within VPC
  ingress {
    from_port   = 9100
    to_port     = 9102
    protocol    = "tcp"
    cidr_blocks = ["10.0.0.0/16"]
    description = "KV Store replication (internal)"
  }

  # Allow all traffic between cluster nodes
  ingress {
    from_port = 0
    to_port   = 0
    protocol  = "-1"
    self      = true
    description = "Cluster internal communication"
  }

  # Outbound internet access
  egress {
    from_port   = 0
    to_port     = 0
    protocol    = "-1"
    cidr_blocks = ["0.0.0.0/0"]
    description = "Allow all outbound"
  }

  tags = {
    Name        = "kv-store-sg"
    Project     = "kv-store"
    Environment = var.environment
  }
}

# Key Pair
resource "aws_key_pair" "kv_store_key" {
  key_name   = "kv-store-key-${var.environment}"
  public_key = var.ssh_public_key

  tags = {
    Name        = "kv-store-key"
    Project     = "kv-store"
    Environment = var.environment
  }
}

# Data source for latest Ubuntu AMI
data "aws_ami" "ubuntu" {
  most_recent = true
  owners      = ["099720109477"] # Canonical

  filter {
    name   = "name"
    values = ["ubuntu/images/hvm-ssd/ubuntu-jammy-22.04-amd64-server-*"]
  }

  filter {
    name   = "virtualization-type"
    values = ["hvm"]
  }
}

data "aws_availability_zones" "available" {
  state = "available"
}

# EC2 Instances with static private IPs
resource "aws_instance" "kv_leader" {
  ami                    = data.aws_ami.ubuntu.id
  instance_type          = var.instance_type
  key_name               = aws_key_pair.kv_store_key.key_name
  subnet_id              = aws_subnet.kv_store_subnet.id
  vpc_security_group_ids = [aws_security_group.kv_store_sg.id]
  private_ip             = "10.0.1.10"

  user_data = templatefile("${path.module}/user-data.sh", {
    node_id  = 1
    role     = "leader"
    peer1_ip = "10.0.1.11"
    peer2_ip = "10.0.1.12"
  })

  root_block_device {
    volume_size           = var.volume_size
    volume_type           = "gp3"
    delete_on_termination = true
    encrypted             = true
  }

  tags = {
    Name        = "kv-store-leader"
    Role        = "leader"
    Project     = "kv-store"
    Environment = var.environment
  }
}

resource "aws_instance" "kv_follower1" {
  ami                    = data.aws_ami.ubuntu.id
  instance_type          = var.instance_type
  key_name               = aws_key_pair.kv_store_key.key_name
  subnet_id              = aws_subnet.kv_store_subnet.id
  vpc_security_group_ids = [aws_security_group.kv_store_sg.id]
  private_ip             = "10.0.1.11"

  user_data = templatefile("${path.module}/user-data.sh", {
    node_id  = 2
    role     = "follower"
    peer1_ip = "10.0.1.10"
    peer2_ip = "10.0.1.12"
  })

  root_block_device {
    volume_size           = var.volume_size
    volume_type           = "gp3"
    delete_on_termination = true
    encrypted             = true
  }

  tags = {
    Name        = "kv-store-follower-1"
    Role        = "follower"
    Project     = "kv-store"
    Environment = var.environment
  }
}

resource "aws_instance" "kv_follower2" {
  ami                    = data.aws_ami.ubuntu.id
  instance_type          = var.instance_type
  key_name               = aws_key_pair.kv_store_key.key_name
  subnet_id              = aws_subnet.kv_store_subnet.id
  vpc_security_group_ids = [aws_security_group.kv_store_sg.id]
  private_ip             = "10.0.1.12"

  user_data = templatefile("${path.module}/user-data.sh", {
    node_id  = 3
    role     = "follower"
    peer1_ip = "10.0.1.10"
    peer2_ip = "10.0.1.11"
  })

  root_block_device {
    volume_size           = var.volume_size
    volume_type           = "gp3"
    delete_on_termination = true
    encrypted             = true
  }

  tags = {
    Name        = "kv-store-follower-2"
    Role        = "follower"
    Project     = "kv-store"
    Environment = var.environment
  }
}
