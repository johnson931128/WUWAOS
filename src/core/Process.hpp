#pragma once

#include <string>

enum class ProcessState {
    New,
    Ready,
    Running,
    Waiting,
    Terminated
};

std::string processStateToString(ProcessState state);

// Minimal process model for the scheduler and process table milestones.
class Process {
public:
    Process(int pid,
            const std::string& name,
            int arrivalTime,
            int burstTime,
            int priority = 0,
            int memoryUsage = 0);

    int getPid() const;
    const std::string& getName() const;
    ProcessState getState() const;
    int getArrivalTime() const;
    int getBurstTime() const;
    int getRemainingTime() const;
    int getPriority() const;
    int getMemoryUsage() const;

    void setState(ProcessState newState);
    void reduceRemainingTime(int amount);

    std::string toString() const;

private:
    int pid;
    std::string name;
    ProcessState state;
    int arrivalTime;
    int burstTime;
    int remainingTime;
    int priority;
    int memoryUsage;
};
