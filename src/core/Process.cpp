#include "core/Process.hpp"

#include <algorithm>
#include <sstream>

Process::Process(int pid,
                 const std::string& name,
                 int arrivalTime,
                 int burstTime,
                 int priority,
                 int memoryUsage)
    : pid(pid),
      name(name),
      state(ProcessState::New),
      arrivalTime(arrivalTime),
      burstTime(burstTime),
      remainingTime(burstTime),
      priority(priority),
      memoryUsage(memoryUsage) {}

int Process::getPid() const {
    return pid;
}

const std::string& Process::getName() const {
    return name;
}

ProcessState Process::getState() const {
    return state;
}

int Process::getArrivalTime() const {
    return arrivalTime;
}

int Process::getBurstTime() const {
    return burstTime;
}

int Process::getRemainingTime() const {
    return remainingTime;
}

int Process::getPriority() const {
    return priority;
}

int Process::getMemoryUsage() const {
    return memoryUsage;
}

void Process::setState(ProcessState newState) {
    state = newState;
}

void Process::reduceRemainingTime(int amount) {
    if (amount <= 0) {
        return;
    }

    remainingTime = std::max(0, remainingTime - amount);
}

std::string Process::toString() const {
    std::ostringstream output;
    output << "Process{"
           << "pid=" << pid
           << ", name=" << name
           << ", state=" << processStateToString(state)
           << ", arrivalTime=" << arrivalTime
           << ", burstTime=" << burstTime
           << ", remainingTime=" << remainingTime
           << ", priority=" << priority
           << ", memoryUsage=" << memoryUsage
           << "}";
    return output.str();
}

std::string processStateToString(ProcessState state) {
    switch (state) {
    case ProcessState::New:
        return "New";
    case ProcessState::Ready:
        return "Ready";
    case ProcessState::Running:
        return "Running";
    case ProcessState::Waiting:
        return "Waiting";
    case ProcessState::Terminated:
        return "Terminated";
    default:
        return "Unknown";
    }
}
