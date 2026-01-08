#include "connection_handler.h"
#include "protocol.h"

#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>

namespace kv {

std::string ProtocolParser::trim(const std::string &s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

CommandType ProtocolParser::parseCommand(const std::string &cmd) {
    std::string upper = cmd;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

    if (upper == "PUT" || upper == "SET")
        return CommandType::PUT;
    if (upper == "GET")
        return CommandType::GET;
    if (upper == "DELETE" || upper == "DEL")
        return CommandType::DELETE;
    if (upper == "PING")
        return CommandType::PING;
    if (upper == "QUIT" || upper == "EXIT")
        return CommandType::QUIT;

    return CommandType::UNKNOWN;
}

std::optional<Request> ProtocolParser::parse(const std::string &line) {
    std::string trimmed = trim(line);
    if (trimmed.empty()) {
        return std::nullopt;
    }

    // Find first space to extract command
    size_t pos = trimmed.find(' ');
    std::string cmdStr;
    std::string rest;

    if (pos == std::string::npos) {
        cmdStr = trimmed;
    } else {
        cmdStr = trimmed.substr(0, pos);
        rest = trim(trimmed.substr(pos + 1));
    }

    CommandType type = parseCommand(cmdStr);
    Request req{type, "", ""};

    switch (type) {
    case CommandType::PUT: {
        // Format: PUT key value (value can contain spaces)
        size_t keyEnd = rest.find(' ');
        if (keyEnd == std::string::npos) {
            // Missing value
            return std::nullopt;
        }
        req.key = rest.substr(0, keyEnd);
        req.value = trim(rest.substr(keyEnd + 1));
        if (req.key.empty() || req.value.empty()) {
            return std::nullopt;
        }
        break;
    }

    case CommandType::GET:
    case CommandType::DELETE: {
        // Format: GET key or DELETE key
        req.key = rest;
        if (req.key.empty()) {
            return std::nullopt;
        }
        break;
    }

    case CommandType::PING:
    case CommandType::QUIT:
        // No arguments needed
        break;

    case CommandType::UNKNOWN:
        // Keep as unknown, will return error
        break;
    }

    return req;
}

ConnectionHandler::ConnectionHandler(int client_fd, CommandExecutor executor) : client_fd_(client_fd), executor_(std::move(executor)) {
}

void ConnectionHandler::run() {
    // Send welcome message
    std::string welcome = "+OK KV-Storage-Engine ready\r\n";
    send(client_fd_, welcome.c_str(), welcome.size(), 0);

    std::string line;
    while (readLine(line)) {
        auto reqOpt = ProtocolParser::parse(line);

        Response response;
        if (!reqOpt.has_value()) {
            response = Response::error("INVALID_COMMAND");
        } else {
            response = executor_(reqOpt.value());

            // Check if client wants to quit
            if (reqOpt->type == CommandType::QUIT) {
                writeResponse(response);
                break;
            }
        }

        if (!writeResponse(response)) {
            break; // Write failed, client disconnected
        }
    }

    std::cout << "[Handler] Client disconnected" << std::endl;
}

bool ConnectionHandler::readLine(std::string &line) {
    line.clear();

    // Prevent buffer from growing unbounded
    const size_t MAX_BUFFER_SIZE = 1024 * 1024; // 1MB
    if (read_buffer_.size() > MAX_BUFFER_SIZE) {
        std::cerr << "[Handler] Read buffer overflow" << std::endl;
        return false;
    }

    size_t pos = read_buffer_.find('\n');
    while (pos == std::string::npos) {
        char buf[4096]; // Increased from 1024
        ssize_t n = recv(client_fd_, buf, sizeof(buf), 0);

        if (n < 0) {
            if (errno == EINTR) {
                continue; // Interrupted, try again
            }
            return false; // Error
        }
        if (n == 0) {
            return false; // Connection closed
        }

        read_buffer_.append(buf, static_cast<size_t>(n));

        if (read_buffer_.size() > MAX_BUFFER_SIZE) {
            return false;
        }

        pos = read_buffer_.find('\n');
    }

    line = read_buffer_.substr(0, pos);
    read_buffer_.erase(0, pos + 1);

    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }

    return true;
}

bool ConnectionHandler::writeResponse(const Response &response) {
    std::string data = response.serialize();
    ssize_t total = 0;
    ssize_t remaining = static_cast<ssize_t>(data.size());

    while (remaining > 0) {
        ssize_t n = send(client_fd_, data.c_str() + total, remaining, 0);
        if (n <= 0) {
            return false;
        }
        total += n;
        remaining -= n;
    }

    return true;
}

} // namespace kv
