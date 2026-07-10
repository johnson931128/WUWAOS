#include "kernel/Kernel.hpp"

Kernel::Kernel() : systemTime(0), nextPid(1), currentRunningPid(-1) {}

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
    Process* process = findProcessByPid(pid);
    if (process == nullptr) {
        return false;
    }

    process->setState(ProcessState::Terminated);
    if (currentRunningPid == pid) {
        currentRunningPid = -1;
    }

    return true;
}

void Kernel::step() {
    systemTime += 1;

    for (auto& process : processes) {
        if (process.getState() == ProcessState::New) {
            process.setState(ProcessState::Ready);
        }
    }

    if (currentRunningPid != -1) {
        Process* runningProcess = findProcessByPid(currentRunningPid);
        if (runningProcess == nullptr ||
            runningProcess->getState() == ProcessState::Terminated ||
            runningProcess->getRemainingTime() <= 0) {
            currentRunningPid = -1;
        }
    }

    if (currentRunningPid == -1) {
        currentRunningPid = scheduler.selectNextPid(processes);
    }

    if (currentRunningPid == -1) {
        return;
    }

    Process* runningProcess = findProcessByPid(currentRunningPid);
    if (runningProcess == nullptr) {
        currentRunningPid = -1;
        return;
    }

    runningProcess->setState(ProcessState::Running);
    runningProcess->reduceRemainingTime(1);

    if (runningProcess->getRemainingTime() == 0) {
        runningProcess->setState(ProcessState::Terminated);
        currentRunningPid = -1;
    }
}

const std::vector<Process>& Kernel::getProcesses() const {
    return processes;
}

int Kernel::getSystemTime() const {
    return systemTime;
}

Process* Kernel::findProcessByPid(int pid) {
    for (auto& process : processes) {
        if (process.getPid() == pid) {
            return &process;
        }
    }

    return nullptr;
}

const Process* Kernel::findProcessByPid(int pid) const {
    for (const auto& process : processes) {
        if (process.getPid() == pid) {
            return &process;
        }
    }

    return nullptr;
}
