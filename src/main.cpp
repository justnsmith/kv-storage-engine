#include "storage_engine.h"

int main() {
    StorageEngine engine("log.bin");
    std::string input;
    engine.recover();
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, input);
        engine.handleCommand(input);
    }
}
