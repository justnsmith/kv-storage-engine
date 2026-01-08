#include "config_parser.h"
#include "tcp_server.h"

#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>

// Global server pointer for signal handling
static kv::TcpServer *g_server = nullptr;

void signalHandler(int signum) {
    std::cout << "\n[Main] Received signal " << signum << ", shutting down..." << std::endl;
    if (g_server) {
        g_server->shutdown();
    }
}

void printUsage(const char *prog) {
    std::cout << "Usage: " << prog << " [options]\n"
              << "Options:\n"
              << "  -f, --config FILE    Config file path (default: server.yaml)\n"
              << "  -p, --port PORT      Port to listen on (default: 6379)\n"
              << "  -h, --host HOST      Host to bind to (default: 0.0.0.0)\n"
              << "  -t, --threads NUM    Number of worker threads (default: 4)\n"
              << "  -c, --cache SIZE     LRU cache size (default: 1000)\n"
              << "  -d, --data DIR       Data directory (default: data)\n"
              << "  --help               Show this help message\n"
              << "\nConfig file (server.yaml) is loaded first, then CLI args override.\n"
              << std::endl;
}

int main(int argc, char *argv[]) {
    std::string configFile;

    // First pass: look for config file argument
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-f" || arg == "--config") && i + 1 < argc) {
            configFile = argv[++i];
        }
    }

    // If no config specified, search in common locations
    if (configFile.empty()) {
        std::vector<std::string> searchPaths = {"server.yaml", "../server.yaml", "../../server.yaml", "../server/server.yaml"};
        auto it =
            std::find_if(searchPaths.begin(), searchPaths.end(), [](const std::string &path) { return std::filesystem::exists(path); });
        if (it != searchPaths.end()) {
            configFile = *it;
        }
    }

    // Load config from file if it exists
    kv::ServerConfig config;
    if (!configFile.empty() && std::filesystem::exists(configFile)) {
        auto loadedConfig = kv::ConfigParser::load(configFile);
        if (loadedConfig.has_value()) {
            config = loadedConfig.value();
            std::cout << "[Config] Loaded from " << configFile << std::endl;
        } else {
            std::cerr << "[Config] Warning: Failed to parse " << configFile << std::endl;
        }
    }

    // Second pass: CLI arguments override config file
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if ((arg == "-f" || arg == "--config") && i + 1 < argc) {
            ++i; // Already handled
        } else if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            config.port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if ((arg == "-h" || arg == "--host") && i + 1 < argc) {
            config.host = argv[++i];
        } else if ((arg == "-t" || arg == "--threads") && i + 1 < argc) {
            config.num_threads = static_cast<size_t>(std::stoi(argv[++i]));
        } else if ((arg == "-c" || arg == "--cache") && i + 1 < argc) {
            config.cache_size = static_cast<size_t>(std::stoi(argv[++i]));
        } else if ((arg == "-d" || arg == "--data") && i + 1 < argc) {
            config.data_dir = argv[++i];
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }

    // Set up signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGPIPE, SIG_IGN);

    std::cout << "========================================" << std::endl;
    std::cout << "   KV Storage Engine Server" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Configuration:" << std::endl;
    std::cout << "  Host:       " << config.host << std::endl;
    std::cout << "  Port:       " << config.port << std::endl;
    std::cout << "  Threads:    " << config.num_threads << std::endl;
    std::cout << "  Cache Size: " << config.cache_size << std::endl;
    std::cout << "  Data Dir:   " << config.data_dir << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        kv::TcpServer server(config);
        g_server = &server;

        server.run();

    } catch (const std::exception &e) {
        std::cerr << "[Main] Fatal error: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "[Main] Server terminated" << std::endl;
    return 0;
}
