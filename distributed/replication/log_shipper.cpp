#include "log_shipper.h"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

namespace distributed {

LogShipper::LogShipper(std::vector<PeerInfo> &peers) : peers_(peers) {
}

LogShipper::~LogShipper() {
    stop();
}

void LogShipper::start() {
    std::cout << "[LogShipper] Started" << std::endl;
}

void LogShipper::stop() {
    shutdown_ = true;

    std::lock_guard<std::mutex> lock(peers_mutex_);
    for (auto &peer : peers_) {
        if (peer.socket_fd >= 0) {
            close(peer.socket_fd);
            peer.socket_fd = -1;
            peer.connected = false;
        }
    }

    std::cout << "[LogShipper] Stopped" << std::endl;
}

void LogShipper::connectToPeers() {
    std::lock_guard<std::mutex> lock(peers_mutex_);

    std::cout << "[LogShipper] Attempting to connect to " << peers_.size() << " peers..." << std::endl;

    for (auto &peer : peers_) {
        if (peer.connected && peer.socket_fd >= 0) {
            std::cout << "[LogShipper] Peer " << peer.host << ":" << peer.port << " already connected (fd=" << peer.socket_fd << ")"
                      << std::endl;
            continue;
        }

        std::cout << "[LogShipper] Connecting to " << peer.host << ":" << peer.port << "..." << std::endl;

        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            std::cerr << "[LogShipper] Failed to create socket: " << strerror(errno) << std::endl;
            continue;
        }

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(peer.port);

        if (inet_pton(AF_INET, peer.host.c_str(), &addr.sin_addr) <= 0) {
            std::cerr << "[LogShipper] Invalid address: " << peer.host << std::endl;
            close(sock);
            continue;
        }

        std::cout << "[LogShipper] Attempting connect to " << peer.host << ":" << peer.port << std::endl;

        if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            std::cerr << "[LogShipper] ✗ Connect failed to " << peer.host << ":" << peer.port << " - " << strerror(errno) << std::endl;
            close(sock);
            peer.socket_fd = -1;
            peer.connected = false;
            continue;
        }

        peer.socket_fd = sock;
        peer.connected = true;
        std::cout << "[LogShipper] ✓ Connected to " << peer.host << ":" << peer.port << " (fd=" << sock << ")" << std::endl;
    }

    int connected_count = 0;
    for (const auto &peer : peers_) {
        if (peer.connected)
            connected_count++;
    }
    std::cout << "[LogShipper] Connection summary: " << connected_count << "/" << peers_.size() << " connected" << std::endl;
}

int LogShipper::shipEntries(const ReplicationMessage &msg) {
    std::string data = msg.serialize();
    data += "\n";

    std::cout << "[LogShipper] Shipping " << msg.entries.size() << " entries to followers..." << std::endl;

    std::lock_guard<std::mutex> lock(peers_mutex_);

    int ack_count = 0;
    for (auto &peer : peers_) {
        std::cout << "[LogShipper] Checking peer " << peer.host << ":" << peer.port << " (connected=" << peer.connected
                  << ", fd=" << peer.socket_fd << ")" << std::endl;

        if (sendToPeer(peer, data)) {
            ack_count++;
            std::cout << "[LogShipper] ✓ ACK from " << peer.host << ":" << peer.port << std::endl;
        } else {
            std::cout << "[LogShipper] ✗ No ACK from " << peer.host << ":" << peer.port << std::endl;
        }
    }

    std::cout << "[LogShipper] Ship complete: " << ack_count << " acks received" << std::endl;

    return ack_count;
}

bool LogShipper::sendToPeer(PeerInfo &peer, const std::string &data) {
    if (!peer.connected || peer.socket_fd < 0) {
        return false;
    }

    ssize_t sent = send(peer.socket_fd, data.c_str(), data.size(), 0);

    if (sent <= 0) {
        peer.connected = false;
        close(peer.socket_fd);
        peer.socket_fd = -1;
        return false;
    }

    char ack_buf[16];
    ssize_t n = recv(peer.socket_fd, ack_buf, sizeof(ack_buf), 0);

    if (n > 0 && strncmp(ack_buf, "+OK", 3) == 0) {
        return true;
    }

    return false;
}

} // namespace distributed
