#pragma once

#include "core/Process.hpp"

#include <string>
#include <vector>

// Minimal kernel skeleton that owns the process table.
class Kernel {
public:
    Kernel();

    int createProcess(const std::string& name,
                      int burstTime,
                      int priority = 0,
                      int memoryUsage = 0);
    bool killProcess(int pid);
    void step();

    const std::vector<Process>& getProcesses() const;
    int getSystemTime() const;

private:
    int systemTime;
    int nextPid;
    std::vector<Process> processes;
};
