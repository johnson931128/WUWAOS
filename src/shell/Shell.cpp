#include "shell/Shell.hpp"

#include <iostream>

void Shell::run() {
    std::string input;
    bool running = true;

    while (running) {
        std::cout << "wuwaos$ ";

        if (!std::getline(std::cin, input)) {
            std::cout << std::endl;
            break;
        }

        if (input.empty()) {
            continue;
        }

        running = handleCommand(input);
    }
}

bool Shell::handleCommand(const std::string& input) {
    if (input == "help") {
        printHelp();
        return true;
    }

    if (input == "clear") {
        clearScreen();
        return true;
    }

    if (input == "exit") {
        return false;
    }

    std::cout << "Unknown command: " << input << std::endl;
    return true;
}

void Shell::printHelp() const {
    std::cout << "Available commands:" << std::endl;
    std::cout << "  help  - Show available commands" << std::endl;
    std::cout << "  clear - Clear the screen or add separation" << std::endl;
    std::cout << "  exit  - Exit the shell" << std::endl;
}

void Shell::clearScreen() const {
    std::cout << std::string(40, '\n');
    std::cout << "---- screen cleared ----" << std::endl;
}
