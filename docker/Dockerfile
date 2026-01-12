# Stage 1: Build environment
FROM ubuntu:22.04 AS builder

# Install basic dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    zlib1g-dev \
    wget \
    && rm -rf /var/lib/apt/lists/*

# Install Go 1.23 for the correct architecture
RUN ARCH=$(dpkg --print-architecture) && \
    wget -q https://go.dev/dl/go1.23.5.linux-${ARCH}.tar.gz && \
    tar -C /usr/local -xzf go1.23.5.linux-${ARCH}.tar.gz && \
    rm go1.23.5.linux-${ARCH}.tar.gz

# Set Go environment
ENV PATH="/usr/local/go/bin:${PATH}"
ENV GOPATH="/go"
ENV GOCACHE="/tmp/go-cache"

# Set working directory
WORKDIR /build

# Copy source code
COPY . .

# Verify Go version
RUN go version

# Build everything
RUN ./scripts/build.sh all Release

# Stage 2: Runtime environment
FROM ubuntu:22.04

# Install runtime dependencies only
RUN apt-get update && apt-get install -y \
    libstdc++6 \
    zlib1g \
    && rm -rf /var/lib/apt/lists/*

# Create app directory and data directories
WORKDIR /app
RUN mkdir -p /app/data

# Copy binaries from builder
COPY --from=builder /build/build/server/kv_server /app/
COPY --from=builder /build/build/cli/kvstore-cli /app/

# Copy configs will be mounted as volumes
RUN mkdir -p /app/config

# Expose ports
# 9000-9002: Client connections
# 9100-9102: Replication connections
EXPOSE 9000 9001 9002 9100 9101 9102

# Default command
CMD ["/app/kv_server", "-f", "/app/config/server.yaml"]
