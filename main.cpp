#include <iostream>
#include <map>

enum Operators { GET, PUT, DELETE, ERROR };

Operators checkValidity(std::string &input, std::string *key, std::string *value) {
    int i = 5;
    if (input.substr(0, 5) == "put(\"") {
        while (input[i] != '\"') {
            *key += input[i];
            i++;
        }
        if (input.substr(++i, 2) != ", ") {
            return ERROR;
        }
        i += 3;
        while (input[i] != '\"') {
            *value += input[i];
            i++;
        }
        if (input.substr(i, 2) != "\")") {
            return ERROR;
        }
        return PUT;

    } else if (input.substr(0, 5) == "get(\"") {
        while (input[i] != '\"') {
            *key += input[i];
            i++;
        }
        if (input.substr(i, 2) != "\")") {
            return ERROR;
        }
        return GET;

    } else if (input.substr(0, 8) == "delete(\"") {
        i = 8;
        while (input[i] != '\"') {
            *key += input[i];
            i++;
        }
        if (input.substr(i, 2) != "\")") {
            return ERROR;
        }
        return DELETE;
    }
    return ERROR;
}

int main() {
    std::map<std::string, std::string> memtable;
    std::string input;
    while (true) {
        std::string key;
        std::string value;
        std::cout << "> ";
        std::getline(std::cin, input, '\n');
        Operators result = checkValidity(input, &key, &value);
        if (result == GET) {
            if (memtable.find(key) != memtable.end()) {
                std::cout << memtable[key] << std::endl;
            } else {
                std::cout << "That key does not exist" << std::endl;
            }
        } else if (result == PUT) {
            memtable[key] = value;
        } else if (result == DELETE) {
            memtable.erase(key);
        } else if (result == ERROR) {
            std::cout << "There was an error." << std::endl;
        }
    }
}
