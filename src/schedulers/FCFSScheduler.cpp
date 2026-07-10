#include "schedulers/FCFSScheduler.hpp"

int FCFSScheduler::selectNextPid(const std::vector<Process>& processes) const {
    const Process* selected = nullptr;

    for (const auto& process : processes) {
        if (process.getState() != ProcessState::Ready) {
            continue;
        }

        if (selected == nullptr ||
            process.getArrivalTime() < selected->getArrivalTime() ||
            (process.getArrivalTime() == selected->getArrivalTime() &&
             process.getPid() < selected->getPid())) {
            selected = &process;
        }
    }

    if (selected == nullptr) {
        return -1;
    }

    return selected->getPid();
}
