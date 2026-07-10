#include "kernel/Kernel.hpp"

Kernel::Kernel() : systemTime(0), nextPid(1) {}

int Kernel::createProcess(const std::string& name,
                          int burstTime,
                          int priority,
                          int memoryUsage) {
    const int pid = nextPid;

    processes.emplace_back(pid, name, systemTime, burstTime, priority, memoryUsage);
    nextPid += 1;

    return pid;
}

bool Kernel::killProcess(int pid) {
    for (auto& process : processes) {
        if (process.getPid() == pid) {
            process.setState(ProcessState::Terminated);
            return true;
        }
    }

    return false;
}

const std::vector<Process>& Kernel::getProcesses() const {
    return processes;
}

int Kernel::getSystemTime() const {
    return systemTime;
}
