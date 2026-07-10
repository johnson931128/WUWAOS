#pragma once

#include "kernel/Kernel.hpp"

#include <string>
#include <vector>

// Thin simulator syscall layer between the shell and kernel state.
class SystemCallInterface {
public:
    int sysRun(const std::string& name,
               int burstTime,
               int priority = 0,
               int memoryUsage = 0);

    bool sysKill(int pid);
    void sysStep();
    const std::vector<Process>& sysPs() const;
    int sysGetTime() const;

private:
    Kernel kernel;
};
