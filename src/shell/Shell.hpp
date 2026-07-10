#pragma once

#include "kernel/Kernel.hpp"

#include <string>

// Minimal terminal shell for the first interactive milestone.
class Shell {
public:
    void run();

private:
    bool handleCommand(const std::string& input);
    void printHelp() const;
    void clearScreen() const;
    void handleRunCommand(const std::string& input);
    void handlePsCommand() const;
    void handleKillCommand(const std::string& input);

    Kernel kernel;
};
