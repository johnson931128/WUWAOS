#pragma once

#include "core/Process.hpp"

#include <vector>

// Base type for future scheduler implementations.
class Scheduler {
public:
    virtual ~Scheduler() = default;

    virtual int selectNextPid(const std::vector<Process>& processes) const = 0;
};
