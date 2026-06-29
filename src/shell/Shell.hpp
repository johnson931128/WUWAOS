#pragma once

#include <string>

// Minimal terminal shell for the first interactive milestone.
class Shell {
public:
    void run();

private:
    bool handleCommand(const std::string& input);
    void printHelp() const;
    void clearScreen() const;
};
