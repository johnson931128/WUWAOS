# Phase 3 - System Time And Step Skeleton

## Goal

Introduce the first explicit notion of simulated time.

This phase adds a minimal `step` command so WUWAOS can move forward one tick at a time without implementing a full scheduler yet.

## Added Data Structures

- Existing `systemTime` in `Kernel`
- Existing process table

## Added Or Updated Methods

- `Kernel::step()`
  - increments simulated time
  - promotes `New` processes to `Ready`
- `SystemCallInterface::sysStep()`
  - forwards the step request to the kernel

## How It Works

When the shell receives `step`, it asks the syscall layer to advance the system by one tick.
The kernel increments `systemTime`, then updates any `New` processes to `Ready`.
This gives the simulator its first visible progression rule without yet deciding which process runs.

## Current Limitations

- No scheduler yet
- `step` does not decrease `remainingTime`
- No `Running` or `Waiting` transitions yet
- No timer-driven preemption

## Next Phase Connection

The next phase should add FCFS scheduling so `step` can start selecting and advancing a process instead of only updating time and readiness.
