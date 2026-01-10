#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <optional>
#include <string>
#include <variant>

namespace kv {

enum class CommandType { PUT, GET, DELETE, PING, QUIT, STATUS, UNKNOWN };

struct Request {
    CommandType type;
    std::string key;
    std::string value; // Only used for PUT
};

struct Response {
    bool success;
    std::string message;
    std::optional<std::string> value; // For GET responses

    static Response ok(const std::string &msg = "OK");
    static Response okWithValue(const std::string &value);
    static Response notFound();
    static Response error(const std::string &msg);
    std::string serialize() const;
};

class ProtocolParser {
  public:
    static std::optional<Request> parse(const std::string &line);

  private:
    static CommandType parseCommand(const std::string &cmd);
    static std::string trim(const std::string &s);
};

} // namespace kv

#endif
