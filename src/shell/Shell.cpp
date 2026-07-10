#include "shell/Shell.hpp"

#include <iomanip>
#include <iostream>
#include <sstream>

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
    if (input == "ps") {
        handlePsCommand();
        return true;
    }

    if (input.rfind("run", 0) == 0) {
        handleRunCommand(input);
        return true;
    }

    if (input.rfind("kill", 0) == 0) {
        handleKillCommand(input);
        return true;
    }

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
    std::cout << "  run <programName> <burstTime> - Create a simulated process" << std::endl;
    std::cout << "  ps    - List simulated processes" << std::endl;
    std::cout << "  kill <pid> - Terminate a simulated process" << std::endl;
    std::cout << "  exit  - Exit the shell" << std::endl;
}

void Shell::clearScreen() const {
    std::cout << std::string(40, '\n');
    std::cout << "---- screen cleared ----" << std::endl;
}

void Shell::handleRunCommand(const std::string& input) {
    std::istringstream stream(input);
    std::string command;
    std::string programName;
    std::string burstTimeText;
    std::string extraToken;

    stream >> command >> programName >> burstTimeText >> extraToken;

    if (programName.empty() || burstTimeText.empty() || !extraToken.empty()) {
        std::cout << "Usage: run <programName> <burstTime>" << std::endl;
        return;
    }

    int burstTime = 0;
    try {
        std::size_t parsedLength = 0;
        burstTime = std::stoi(burstTimeText, &parsedLength);
        if (parsedLength != burstTimeText.size()) {
            std::cout << "Error: burstTime must be a positive integer." << std::endl;
            return;
        }
    } catch (...) {
        std::cout << "Error: burstTime must be a positive integer." << std::endl;
        return;
    }

    if (burstTime <= 0) {
        std::cout << "Error: burstTime must be greater than 0." << std::endl;
        return;
    }

    const int pid = syscalls.sysRun(programName, burstTime);
    std::cout << "Created process PID=" << pid
              << " NAME=" << programName << std::endl;
}

void Shell::handlePsCommand() const {
    const auto& processes = syscalls.sysPs();

    if (processes.empty()) {
        std::cout << "No processes." << std::endl;
        return;
    }

    std::cout << std::left
              << std::setw(6) << "PID"
              << std::setw(10) << "NAME"
              << std::setw(14) << "STATE"
              << std::setw(10) << "ARRIVAL"
              << std::setw(8) << "BURST"
              << std::setw(10) << "REMAINING"
              << std::endl;

    for (const auto& process : processes) {
        std::cout << std::left
                  << std::setw(6) << process.getPid()
                  << std::setw(10) << process.getName()
                  << std::setw(14) << processStateToString(process.getState())
                  << std::setw(10) << process.getArrivalTime()
                  << std::setw(8) << process.getBurstTime()
                  << std::setw(10) << process.getRemainingTime()
                  << std::endl;
    }
}

void Shell::handleKillCommand(const std::string& input) {
    std::istringstream stream(input);
    std::string command;
    std::string pidText;
    std::string extraToken;

    stream >> command >> pidText >> extraToken;

    if (pidText.empty() || !extraToken.empty()) {
        std::cout << "Usage: kill <pid>" << std::endl;
        return;
    }

    int pid = 0;
    try {
        std::size_t parsedLength = 0;
        pid = std::stoi(pidText, &parsedLength);
        if (parsedLength != pidText.size()) {
            std::cout << "Error: pid must be a positive integer." << std::endl;
            return;
        }
    } catch (...) {
        std::cout << "Error: pid must be a positive integer." << std::endl;
        return;
    }

    if (pid <= 0) {
        std::cout << "Error: pid must be greater than 0." << std::endl;
        return;
    }

    if (!syscalls.sysKill(pid)) {
        std::cout << "Error: process not found." << std::endl;
        return;
    }

    std::cout << "Terminated process PID=" << pid << std::endl;
}
