#ifndef CONNECTION_HANDLER_H
#define CONNECTION_HANDLER_H

#include "protocol.h"
#include <functional>
#include <string>

namespace kv {

class ConnectionHandler {
  public:
    using CommandExecutor = std::function<Response(const Request &)>;
    ConnectionHandler(int client_fd, CommandExecutor executor);
    void run();

  private:
    bool readLine(std::string &line);
    bool writeResponse(const Response &response);

    int client_fd_;
    CommandExecutor executor_;
    std::string read_buffer_;
};

} // namespace kv

#endif
