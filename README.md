# LSM-Based Distributed Key-Value Store

A high-performance, distributed key-value storage system with LSM-tree architecture, write-ahead logging, and leader-follower replication. Built from scratch in C++ and Go, deployable to AWS EC2.

[![C++ CI](https://github.com/justnsmith/kv-store/workflows/C%++%20&%20Go%20CI/badge.svg)](https://github.com/justnsmith/kv-store/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

## Table of Contents

- [Features](#features)
- [Quick Start](#quick-start)
- [Local Development](#local-development)
- [Docker Deployment](#docker-deployment)
- [AWS Deployment](#aws-deployment)
- [Usage](#usage)
- [Design Decisions](#design-decisions)
- [Performance](#performance)
- [Testing](#testing)
- [Project Structure](#project-structure)
- [Known Limitations](#known-limitations)

## Features

### Storage Engine
- **LSM-Tree Architecture** with multi-level compaction
- **Write-Ahead Log (WAL)** with background fsync for durability
- **In-memory MemTable** with automatic flushing to disk
- **SSTable** files with bloom filters for fast lookups
- **LRU Cache** for hot data with configurable size
- **Async Write Queue** for non-blocking operations

### Distributed System
- **Leader-Follower Replication** with log shipping
- **3-Node Cluster** with automatic failover detection
- **Strong Consistency** via log sequence numbers
- **Write Rejection** on followers (leader-only writes)

### Networking & Operations
- **Multi-threaded TCP Server** with thread pool
- **Custom Protocol** with efficient serialization
- **Go CLI** for human-friendly interaction
- **Docker Support** with multi-stage builds
- **Terraform IaC** for AWS deployment

### Write Path
1. Client sends PUT/DELETE to leader
2. Leader appends to WAL (durability)
3. Leader inserts into MemTable (memory)
4. Leader replicates log entry to followers
5. Followers ACK receipt
6. Leader commits and responds to client
7. Background: MemTable flush → SSTable
8. Background: Compaction merges SSTables

### Read Path
1. Client sends GET to any node
2. Check LRU cache (fast path)
3. Check MemTable (memory)
4. Check immutable MemTable (if flushing)
5. Check SSTables with bloom filters
6. Return value or NOT_FOUND

## Quick Start

### Prerequisites
- **C++ Compiler**: g++ or clang with C++17 support
- **CMake**: 3.10 or higher
- **Go**: 1.23+ (for CLI)
- **Docker**: 20.10+ (optional, for containers)
- **Terraform**: 1.0+ (optional, for AWS deployment)

### Build Everything

```bash
# Clone the repository
git clone https://github.com/justnsmith/kv-store.git
cd kv-store

# Build all components
./scripts/build.sh all Release

# Run tests
./scripts/test.sh

# Run the server
./build/server/kv_server -f server/server1.yaml
```

### Quick Test

```bash
# In another terminal, use the CLI
./build/cli/kvstore-cli set mykey myvalue
./build/cli/kvstore-cli get mykey
./build/cli/kvstore-cli del mykey
```

## Local Development

### Building Individual Components

```bash
# Build only the engine
./scripts/build.sh engine

# Build only the server
./scripts/build.sh server

# Build only the CLI
./scripts/build.sh cli

# Build with debug symbols
./scripts/build.sh all Debug
```

### Running the Server

```bash
# Run with default config
./build/server/kv_server

# Run with custom config
./build/server/kv_server -f server/server1.yaml

# Command line options
./build/server/kv_server --help
```

### Using the CLI

```bash
# Connect to default server (localhost:9000)
./build/cli/kvstore-cli ping

# Connect to remote server
./build/cli/kvstore-cli --host 192.168.1.100 --port 9000 get mykey

# Set a value
./build/cli/kvstore-cli set key1 value1

# Get a value
./build/cli/kvstore-cli get key1

# Delete a value
./build/cli/kvstore-cli del key1

# Check server status
./build/cli/kvstore-cli status

# Configure default host/port
./build/cli/kvstore-cli config set host 192.168.1.100
./build/cli/kvstore-cli config set port 9000
```

### Running Tests

```bash
# Run all tests
./scripts/test.sh

# Run specific test suite
./build/engine/engine_tests

# Run with valgrind (memory leak detection)
./scripts/run.sh tests --valgrind

# Run benchmarks
./scripts/run.sh benchmark --benchmark write_throughput
./scripts/run.sh benchmark --benchmark read_latency
./scripts/run.sh benchmark --benchmark compaction
```

## Docker Deployment

### Local 3-Node Cluster

```bash
# Build Docker images
./scripts/run.sh docker:build

# Start the cluster
./scripts/run.sh docker:up

# View logs
./scripts/run.sh docker:logs

# Stop the cluster
./scripts/run.sh docker:down
```

The cluster will be available at:
- **Leader**: http://localhost:9000
- **Follower 1**: http://localhost:9001
- **Follower 2**: http://localhost:9002

### Test Replication

```bash
# Write to leader
echo "PUT test value" | nc localhost 9000

# Read from followers
echo "GET test" | nc localhost 9001
echo "GET test" | nc localhost 9002
```

## AWS Deployment

### Prerequisites

1. **AWS Account** with appropriate permissions
2. **AWS CLI** configured with credentials
3. **SSH Key** generated for EC2 access

### Setup

```bash
# Generate SSH key
ssh-keygen -t ed25519 -f ~/.ssh/kv-store -C "kv-store"

# Configure AWS credentials
aws configure

# Create Terraform variables
cd terraform
cp terraform.tfvars.example terraform.tfvars
# Edit terraform.tfvars with your settings
```

### Deploy to AWS

```bash
# Initialize Terraform
./scripts/deploy.sh init

# Preview deployment
./scripts/deploy.sh plan

# Deploy cluster (creates 3 EC2 instances)
./scripts/deploy.sh apply

# Wait ~5 minutes for initialization
```

### Access Your Cluster

```bash
# SSH to nodes
./scripts/deploy.sh ssh leader
./scripts/deploy.sh ssh follower1
./scripts/deploy.sh ssh follower2

# View logs
./scripts/deploy.sh logs leader

# Check cluster status
./scripts/deploy.sh status
```

### Test Your AWS Cluster

```bash
# Get leader IP
LEADER_IP=$(cd terraform && terraform output -raw leader_public_ip)

# Test operations
echo "PING" | nc $LEADER_IP 9000
echo "PUT mykey myvalue" | nc $LEADER_IP 9000
echo "GET mykey" | nc $LEADER_IP 9000

# Or use the CLI
./build/cli/kvstore-cli --host $LEADER_IP --port 9000 set test value123
./build/cli/kvstore-cli --host $LEADER_IP --port 9000 get test
```

### Cleanup

```bash
# Destroy all AWS resources
./scripts/deploy.sh destroy
```

**Important**: Always destroy resources when done to avoid AWS charges!

## Usage

### Server Configuration

Edit `server/server1.yaml`:

```yaml
node:
  id: 1
  role: "leader"  # or "follower"
server:
  host: "0.0.0.0"
  port: 9000
  threads: 4
replication:
  port: 9100
peers:
  - host: "10.0.1.11"
    port: 9101
  - host: "10.0.1.12"
    port: 9102
storage:
  data_dir: "/app/data"
  cache_size: 1000
logging:
  level: "info"
```

### Protocol

The server uses a simple text-based protocol:

```
PUT <key> <value>    # Set a key-value pair
GET <key>            # Retrieve a value
DELETE <key>         # Delete a key
PING                 # Health check
STATUS               # Server statistics
QUIT                 # Close connection
```

### Example Session

```bash
$ telnet localhost 9000
> PUT hello world
+OK
> GET hello
world
> DELETE hello
+OK
> GET hello
-NOT_FOUND
> STATUS
Node ID: 1
Role: LEADER
Term: 0
Commit Index: 5
Active Connections: 1
> QUIT
+OK
```

## Design Decisions

### Why C++ for the Storage Engine?

- **Performance**: Direct memory management and zero-cost abstractions
- **Control**: Fine-grained control over memory layout and I/O
- **Industry Standard**: Most production databases (RocksDB, LevelDB) use C++
- **Learning**: Deep understanding of low-level systems programming

### Why Go for the CLI?

- **Rapid Development**: Fast iteration on user-facing features
- **Cross-Platform**: Easy compilation for multiple OS/architectures
- **Standard Library**: Built-in HTTP, JSON, and networking support
- **Separation of Concerns**: Keep client logic separate from engine

### Write-Ahead Log Design

- **Group Commit**: Batches writes for efficiency
- **Background Sync**: fsync() in separate thread (10ms interval)
- **Checksums**: CRC32 for corruption detection
- **Sequential Writes**: Optimal for HDD/SSD performance

### Compaction Strategy

- **Level 0**: Size-based (4 SSTables trigger compaction)
- **Level 1+**: Size-tiered with 10x growth per level
- **Background Thread**: Never blocks client operations
- **Bloom Filters**: Skip SSTables that definitely don't contain key

## Performance

### Benchmarks (Local Machine)

**Hardware**: MacBook Pro M3, 16GB RAM, SSD

```
Write Throughput:   ~50,000 ops/sec
Read Latency:       ~50 microseconds (cache hit)
Read Latency:       ~500 microseconds (disk)
Compaction Speed:   ~100 MB/sec
```

### Optimization Techniques

1. **Write Batching**: WriteQueue groups operations
2. **Bloom Filters**: 1% false positive rate, saves disk I/O
3. **LRU Cache**: 80%+ hit rate on typical workloads
4. **Async I/O**: Non-blocking writes via background threads
5. **Zero-Copy**: mmap for SSTable reads (future work)

### Scalability

- **Storage**: Limited by disk space
- **Memory**: ~1KB per key in MemTable
- **Throughput**: Scales with number of cores (thread pool)
- **Cluster**: Linear read scaling with follower count

## Testing

### Unit Tests

```bash
# Run all tests
./scripts/test.sh

# Run specific test
./build/engine/engine_tests --gtest_filter=MemTableTest.*
```

### Integration Tests

```bash
# Test 3-node cluster locally
./scripts/run.sh cluster

# In another terminal
./build/cli/kvstore-cli --port 9000 set test1 value1
./build/cli/kvstore-cli --port 9001 get test1  # Should work (replicated)
./build/cli/kvstore-cli --port 9001 set test2 value2  # Should fail (follower)
```

### CI/CD

GitHub Actions automatically:
- ✅ Builds all components
- ✅ Runs unit tests
- ✅ Runs static analysis (cppcheck)
- ✅ Checks code formatting (clang-format)
- ✅ Builds and tests Docker images
- ✅ Verifies cluster connectivity

## Project Structure

```
kv-store/
├── engine/              # C++ LSM-tree storage engine
│   ├── include/         # Header files
│   ├── src/             # Implementation
│   ├── tests/           # Unit tests
│   └── benchmarks/      # Performance benchmarks
├── server/              # TCP server with replication
│   ├── include/         # Server headers
│   └── src/             # Server implementation
├── distributed/         # Replication layer
│   ├── replication/     # Leader/follower logic
│   └── failure/         # Heartbeat detection
├── cli/                 # Go command-line interface
│   ├── cmd/             # CLI entry point
│   └── internal/        # CLI implementation
├── docker/              # Docker configuration
│   ├── Dockerfile       # Multi-stage build
│   ├── docker-compose.yml  # 3-node cluster
│   └── configs/         # Node configurations
├── terraform/           # AWS infrastructure
│   ├── main.tf          # EC2, VPC, security groups
│   ├── variables.tf     # Configuration variables
│   └── outputs.tf       # Cluster endpoints
└── scripts/             # Build and deployment scripts
    ├── build.sh         # Build system
    ├── test.sh          # Test runner
    ├── docker-*.sh      # Docker commands
    └── deploy.sh        # AWS deployment
```

## Known Limitations

1. **No Leader Election**: Leader must be manually designated
2. **Fixed Cluster Size**: 3 nodes only (not dynamic)
3. **Sequential Log Replay**: Followers must receive all entries in order
4. **No Authentication**: Insecure (use VPC/firewall in production)
5. **Single-Key Operations**: No batch operations or transactions
6. **In-Memory Bloom Filters**: Not persisted (rebuilt on startup)

## Future Improvements

- [ ] Implement Raft consensus for leader election
- [ ] Add range queries and scans
- [ ] Implement MVCC for snapshot isolation
- [ ] Add Prometheus metrics endpoint
- [ ] Implement backup and restore
- [ ] Support dynamic cluster membership
- [ ] Add TLS/SSL encryption
- [ ] Implement read replicas
- [ ] Add query optimizer
- [ ] Support secondary indexes

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgments

- Inspired by [LevelDB](https://github.com/google/leveldb) and [RocksDB](https://github.com/facebook/rocksdb)
- LSM-tree design from the [Bigtable paper](https://research.google/pubs/pub27898/)
- Replication concepts from [Raft](https://raft.github.io/)
