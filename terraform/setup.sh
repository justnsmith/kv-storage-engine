#!/bin/bash
set -e

# Update system
apt-get update
apt-get upgrade -y

# Install Docker
curl -fsSL https://get.docker.com -o get-docker.sh
sh get-docker.sh
usermod -aG docker ubuntu

# Install Docker Compose
mkdir -p /usr/local/lib/docker/cli-plugins
curl -SL https://github.com/docker/compose/releases/download/v2.24.0/docker-compose-linux-x86_64 -o /usr/local/lib/docker/cli-plugins/docker-compose
chmod +x /usr/local/lib/docker/cli-plugins/docker-compose

# Create app directory
mkdir -p /opt/kv-store/data
mkdir -p /opt/kv-store/config

# Create configuration script that will be called later with IPs
cat > /tmp/configure-node.sh << 'CONFIGURE_SCRIPT'
#!/bin/bash
NODE_ID=$1
ROLE=$2
PEER1_IP=$3
PEER2_IP=$4

cd /opt/kv-store

# Create server config based on role
if [ "$ROLE" = "leader" ]; then
cat > /opt/kv-store/config/server.yaml << EOF
node:
  id: ${NODE_ID}
  role: "leader"
server:
  host: "0.0.0.0"
  port: 9000
  threads: 4
replication:
  port: 9100
peers:
  - host: "${PEER1_IP}"
    port: 9101
  - host: "${PEER2_IP}"
    port: 9102
storage:
  data_dir: "/app/data"
  cache_size: 1000
logging:
  level: "info"
EOF
else
cat > /opt/kv-store/config/server.yaml << EOF
node:
  id: ${NODE_ID}
  role: "follower"
server:
  host: "0.0.0.0"
  port: 900$((NODE_ID-1))
  threads: 4
replication:
  port: 910$((NODE_ID-1))
peers:
  - host: "${PEER1_IP}"
    port: 9100
  - host: "${PEER2_IP}"
    port: 910$((NODE_ID==2?2:1))
storage:
  data_dir: "/app/data"
  cache_size: 1000
logging:
  level: "info"
EOF
fi

# Pull or build Docker image
# For now, we'll build a simple image
cat > /opt/kv-store/Dockerfile << 'DOCKERFILE'
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    zlib1g-dev \
    wget \
    && rm -rf /var/lib/apt/lists/*

RUN ARCH=$(dpkg --print-architecture) && \
    wget -q https://go.dev/dl/go1.23.5.linux-${ARCH}.tar.gz && \
    tar -C /usr/local -xzf go1.23.5.linux-${ARCH}.tar.gz && \
    rm go1.23.5.linux-${ARCH}.tar.gz

ENV PATH="/usr/local/go/bin:${PATH}"
ENV GOPATH="/go"

WORKDIR /build
COPY . .
RUN ./scripts/build.sh all Release

FROM ubuntu:22.04
RUN apt-get update && apt-get install -y libstdc++6 zlib1g && rm -rf /var/lib/apt/lists/*
WORKDIR /app
RUN mkdir -p /app/data
COPY --from=0 /build/build/server/kv_server /app/
COPY --from=0 /build/build/cli/kvstore-cli /app/
EXPOSE 9000 9001 9002 9100 9101 9102
CMD ["/app/kv_server", "-f", "/app/config/server.yaml"]
DOCKERFILE

# Stop existing container if running
docker stop kv-store 2>/dev/null || true
docker rm kv-store 2>/dev/null || true

# Run container
docker run -d \
  --name kv-store \
  --restart unless-stopped \
  -p 900$((NODE_ID-1)):900$((NODE_ID-1)) \
  -p 910$((NODE_ID-1)):910$((NODE_ID-1)) \
  -v /opt/kv-store/config:/app/config:ro \
  -v /opt/kv-store/data:/app/data \
  YOUR_DOCKERHUB_USERNAME/kv-store:latest || echo "Failed to start container - will need manual setup"

echo "Node ${NODE_ID} (${ROLE}) configured"
CONFIGURE_SCRIPT

chmod +x /tmp/configure-node.sh

echo "Initial setup complete. Waiting for Terraform to provide peer IPs..."
