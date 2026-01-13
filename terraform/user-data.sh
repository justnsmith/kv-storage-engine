#!/bin/bash
set -e

apt-get update
apt-get upgrade -y

curl -fsSL https://get.docker.com -o get-docker.sh
sh get-docker.sh
usermod -aG docker ubuntu

mkdir -p /usr/local/lib/docker/cli-plugins
curl -SL https://github.com/docker/compose/releases/download/v2.24.0/docker-compose-linux-x86_64 -o /usr/local/lib/docker/cli-plugins/docker-compose
chmod +x /usr/local/lib/docker/cli-plugins/docker-compose

mkdir -p /opt/kv-store/data /opt/kv-store/config
cd /opt/kv-store

cat > /opt/kv-store/config/server.yaml << 'EOF'
node:
  id: ${node_id}
  role: "${role}"
server:
  host: "0.0.0.0"
  port: ${ 9000 + node_id - 1 }
  threads: 4
replication:
  port: ${ 9100 + node_id - 1 }
peers:
%{ if role == "leader" ~}
  - host: "${peer1_ip}"
    port: 9101
  - host: "${peer2_ip}"
    port: 9102
%{ else ~}
  - host: "${peer1_ip}"
    port: ${ node_id == 2 ? 9100 : 9100 }
  - host: "${peer2_ip}"
    port: ${ node_id == 2 ? 9102 : 9101 }
%{ endif ~}
storage:
  data_dir: "/app/data"
  cache_size: 1000
logging:
  level: "info"
EOF
