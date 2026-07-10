#pragma once

#include "schedulers/Scheduler.hpp"

// Minimal FCFS scheduler that chooses the earliest ready process.
class FCFSScheduler : public Scheduler {
public:
    int selectNextPid(const std::vector<Process>& processes) const override;
};
