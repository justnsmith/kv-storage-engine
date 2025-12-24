#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

enum class Operation { GET = 0, PUT = 1, DELETE = 2, ERROR = 3 };

// Parse input commands like: put("key", "value"), get("key"), delete("key")
Operation parseCommand(const std::string &input, std::string &key, std::string &value) {
    key.clear();
    value.clear();

    if (input.substr(0, 4) == "put(") {
        size_t startKey = input.find('"');
        size_t endKey = input.find('"', startKey + 1);
        if (startKey == std::string::npos || endKey == std::string::npos)
            return Operation::ERROR;

        key = input.substr(startKey + 1, endKey - startKey - 1);

        size_t startValue = input.find('"', endKey + 1);
        size_t endValue = input.find('"', startValue + 1);
        if (startValue == std::string::npos || endValue == std::string::npos)
            return Operation::ERROR;

        value = input.substr(startValue + 1, endValue - startValue - 1);
        return Operation::PUT;

    } else if (input.substr(0, 4) == "get(") {
        size_t startKey = input.find('"');
        size_t endKey = input.find('"', startKey + 1);
        if (startKey == std::string::npos || endKey == std::string::npos)
            return Operation::ERROR;

        key = input.substr(startKey + 1, endKey - startKey - 1);
        return Operation::GET;

    } else if (input.substr(0, 7) == "delete(") {
        size_t startKey = input.find('"');
        size_t endKey = input.find('"', startKey + 1);
        if (startKey == std::string::npos || endKey == std::string::npos)
            return Operation::ERROR;

        key = input.substr(startKey + 1, endKey - startKey - 1);
        return Operation::DELETE;
    }

    return Operation::ERROR;
}

void writeLog(Operation op, const std::string &key, const std::string &value) {
    std::ofstream logFile("log.txt", std::ios::app);
    if (!logFile) {
        std::cerr << "Failed to open log file!" << std::endl;
        return;
    }

    // Format: op "key" "value"
    logFile << static_cast<int>(op) << " \"" << key << "\" \"" << value << "\"\n";
}

// Load log and rebuild memtable
void loadLog(const std::string &fileName, std::map<std::string, std::string> &memtable) {
    std::ifstream logFile(fileName);
    if (!logFile)
        return;

    std::string line;
    while (std::getline(logFile, line)) {
        std::istringstream iss(line);
        int opInt;
        char quote;

        if (!(iss >> opInt >> quote) || quote != '"')
            continue;

        std::string key;
        if (!std::getline(iss, key, '"'))
            continue;

        if (!(iss >> quote) || quote != '"')
            continue;

        std::string value;
        if (!std::getline(iss, value, '"'))
            continue;

        Operation op = static_cast<Operation>(opInt);
        if (op == Operation::PUT) {
            memtable[key] = value;
        } else if (op == Operation::DELETE) {
            memtable.erase(key);
        }
    }
}

int main() {
    std::map<std::string, std::string> memtable;
    loadLog("log.txt", memtable);

    std::string input;
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, input);

        std::string key, value;
        Operation op = parseCommand(input, key, value);

        switch (op) {
        case Operation::GET:
            if (memtable.find(key) != memtable.end())
                std::cout << memtable[key] << std::endl;
            else
                std::cout << "Key not found" << std::endl;
            break;

        case Operation::PUT:
            writeLog(op, key, value);
            memtable[key] = value;
            break;

        case Operation::DELETE:
            writeLog(op, key, value);
            memtable.erase(key);
            break;

        case Operation::ERROR:
        default:
            std::cerr << "Invalid command" << std::endl;
        }
    }
}
