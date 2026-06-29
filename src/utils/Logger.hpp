#pragma once

#include <iostream>
#include <string>

// Small helper for future terminal messages.
struct Logger {
    static void info(const std::string& message) {
        std::cout << message << std::endl;
    }
};
