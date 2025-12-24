#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

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

void writeLog(Operators op, std::string &key, std::string &value) {
    std::ofstream outputFile("log.txt", std::ios::app);

    if (!outputFile.is_open()) {
        std::cerr << "Error: Could not open the file." << std::endl;
        return;
    }
    outputFile << op << " " << key << " " << value << std::endl;
    outputFile.close();
    std::cout << "Data written to log.txt successfully." << std::endl;
}

void loadLog(const std::string &fileName, std::map<std::string, std::string> *memtable) {
    std::ifstream inputFile(fileName);

    if (!inputFile.is_open()) {
        std::cerr << "Error: Could not open the file " << fileName << std::endl;
        return;
    }
    std::string line;
    while (std::getline(inputFile, line)) {
        Operators op = static_cast<Operators>(line[0] - '0');
        std::istringstream iss(line.substr(2));
        std::string key;
        std::string value;

        iss >> key;
        std::getline(iss, value);

        if (op == PUT) {
            (*memtable)[key] = value;
        } else if (op == DELETE) {
            (*memtable).erase(key);
        }
    }
}

int main() {
    const std::string logPath = "log.txt";
    std::map<std::string, std::string> memtable;
    std::string input;
    loadLog(logPath, &memtable);
    while (true) {
        std::string key;
        std::string value;
        std::cout << "> ";
        std::getline(std::cin, input, '\n');
        Operators result = checkValidity(input, &key, &value);
        if (result != ERROR && result != GET) {
            writeLog(result, key, value);
        }

        switch (result) {
        case (GET):
            if (memtable.find(key) != memtable.end()) {
                std::cout << memtable[key] << std::endl;
            } else {
                std::cout << "That key does not exist" << std::endl;
            }
            break;

        case (PUT):
            memtable[key] = value;
            break;

        case (DELETE):
            memtable.erase(key);
            break;

        case (ERROR):
            std::cerr << "There was an error." << std::endl;
            break;
        }
    }
}
