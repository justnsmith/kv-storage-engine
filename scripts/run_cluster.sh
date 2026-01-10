#!/usr/bin/env bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "========================================="
echo "  Starting 3-Node KV Cluster"
echo "========================================="

# Build first
echo "→ Building server..."
"$SCRIPT_DIR/build.sh" server || exit 1

# Create data directories IN BUILD FOLDER
mkdir -p "$ROOT_DIR/build/data1" "$ROOT_DIR/build/data2" "$ROOT_DIR/build/data3"

# Start FOLLOWERS first (so they're listening when leader connects)
echo ""
echo "→ Starting Node 2 (FOLLOWER) on port 9001..."
"$ROOT_DIR/build/server/kv_server" -f "$ROOT_DIR/server/server2.yaml" &
PID2=$!
sleep 2

echo "→ Starting Node 3 (FOLLOWER) on port 9002..."
"$ROOT_DIR/build/server/kv_server" -f "$ROOT_DIR/server/server3.yaml" &
PID3=$!
sleep 2

# Now start LEADER (it will connect to followers)
echo "→ Starting Node 1 (LEADER) on port 9000..."
"$ROOT_DIR/build/server/kv_server" -f "$ROOT_DIR/server/server1.yaml" &
PID1=$!
sleep 2

echo ""
echo "========================================="
echo "  Cluster Running"
echo "========================================="
echo "  Node 1 (Leader):    localhost:9000 (replication: 9100)"
echo "  Node 2 (Follower):  localhost:9001 (replication: 9101)"
echo "  Node 3 (Follower):  localhost:9002 (replication: 9102)"
echo ""
echo "  PIDs: $PID1, $PID2, $PID3"
echo ""
echo "Press Ctrl+C to stop all nodes"
echo "========================================="

# Wait and cleanup on exit
trap "echo ''; echo 'Shutting down cluster...'; kill $PID1 $PID2 $PID3 2>/dev/null; wait; echo 'Cluster stopped.'; exit" INT TERM

wait
