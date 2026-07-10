#include "system/SystemCallInterface.hpp"

int SystemCallInterface::sysRun(const std::string& name,
                                int burstTime,
                                int priority,
                                int memoryUsage) {
    return kernel.createProcess(name, burstTime, priority, memoryUsage);
}

bool SystemCallInterface::sysKill(int pid) {
    return kernel.killProcess(pid);
}

void SystemCallInterface::sysStep() {
    kernel.step();
}

const std::vector<Process>& SystemCallInterface::sysPs() const {
    return kernel.getProcesses();
}

int SystemCallInterface::sysGetTime() const {
    return kernel.getSystemTime();
}
