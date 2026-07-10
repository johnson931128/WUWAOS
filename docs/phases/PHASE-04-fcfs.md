# Phase 4 - FCFS Scheduler Baseline

## Goal

Make `step` execute a real process tick using FCFS selection.

This is the first phase where WUWAOS stops being only a process registry and starts behaving like a scheduler-driven simulator.

## Added Data Structures

- `FCFSScheduler`
- `currentRunningPid` in `Kernel`

## Added Or Updated Methods

- `Scheduler::selectNextPid(...)`
  - abstract scheduler selection interface
- `FCFSScheduler::selectNextPid(...)`
  - picks the earliest ready process
- `Kernel::findProcessByPid(...)`
  - internal helper for locating a process
- updated `Kernel::step()`
  - advances time
  - promotes new processes to ready
  - selects a process with FCFS
  - runs one tick
  - terminates the process when remaining time reaches zero

## How It Works

When `step` runs, the kernel first increments time and promotes `New` processes to `Ready`.
If no process is currently running, the kernel asks `FCFSScheduler` to choose the earliest ready process.
The selected process becomes `Running`, loses one unit of `remainingTime`, and stays running until it finishes.
When `remainingTime` reaches zero, the kernel marks the process as `Terminated`.

## Current Limitations

- Only FCFS is implemented
- No Round Robin quantum
- No waiting or I/O wakeup behavior
- `step` output is still textual and minimal

## Next Phase Connection

The next phase should add Round Robin scheduling and simple execution metrics such as waiting time and turnaround time.
