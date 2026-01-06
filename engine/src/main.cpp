#include "engine.h"

int main() {
    StorageEngine engine("data/log.bin");
    std::string input;
    engine.recover();
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, input);
        engine.handleCommand(input);
    }
}
