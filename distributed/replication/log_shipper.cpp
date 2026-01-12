#include "log_shipper.h"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netdb.h>
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

    for (size_t i = 0; i < peers_.size(); i++) {
        std::cout << "[LogShipper] DEBUG: Peer " << i << " = " << peers_[i].host << ":" << peers_[i].port << std::endl;
    }

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

        struct addrinfo hints, *result;
        std::memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET; // IPv4
        hints.ai_socktype = SOCK_STREAM;

        std::string port_str = std::to_string(peer.port);
        int status = getaddrinfo(peer.host.c_str(), port_str.c_str(), &hints, &result);

        if (status != 0) {
            std::cerr << "[LogShipper] Failed to resolve " << peer.host << ": " << gai_strerror(status) << std::endl;
            close(sock);
            continue;
        }

        std::cout << "[LogShipper] Attempting connect to " << peer.host << ":" << peer.port << std::endl;

        // Try to connect using the resolved address
        if (connect(sock, result->ai_addr, result->ai_addrlen) < 0) {
            std::cerr << "[LogShipper] ✗ Connect failed to " << peer.host << ":" << peer.port << " - " << strerror(errno) << std::endl;
            freeaddrinfo(result);
            close(sock);
            peer.socket_fd = -1;
            peer.connected = false;
            continue;
        }

        freeaddrinfo(result);

        peer.socket_fd = sock;
        peer.connected = true;
        std::cout << "[LogShipper] ✓ Connected to " << peer.host << ":" << peer.port << " (fd=" << sock << ")" << std::endl;
    }

    int connected_count = std::count_if(peers_.begin(), peers_.end(), [](const PeerInfo &peer) { return peer.connected; });
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
