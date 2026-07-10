# WUWAOS Implementation Plan

This roadmap adapts the main xv6 topics to a terminal-first OS simulator.
WUWAOS is not a bootable OS, so each phase translates xv6 concepts into small, inspectable simulator milestones.

## GitHub Upload Cadence

For every phase:

1. Implement only the current phase.
2. Build with:
   ```powershell
   cmake -S . -B build
   cmake --build build
   ```
3. Run a small manual verification for the new behavior.
4. Update:
   - `docs/PROGRESS.md`
   - `README.md` when user-visible behavior changes
   - the phase document in `docs/phases/`
5. Stage only the files for that phase.
6. Commit with the planned commit message.
7. Push to `origin/main`.

Recommended rhythm: one push per completed phase, never mix two phases into one commit.

## Phase 1: Process Control Baseline

- Goal:
  Stabilize the existing shell-to-kernel process flow and complete the first user-visible process lifecycle commands.
- xv6 mapping:
  Chapter 1 (interfaces), Chapter 2 (kernel organization), Chapter 7 (wait/exit/kill).
- Features:
  - Stable `run <programName> <burstTime>`
  - Stable `ps`
  - New `kill <pid>`
  - Shell remains a thin parser and presenter
- Main data structures:
  - `Process`
  - `std::vector<Process>` process table in `Kernel`
- Methods:
  - Add `Kernel::killProcess(int pid)`
  - Complete `Shell` command handlers for `run`, `ps`, `kill`
- Files:
  - `src/kernel/Kernel.hpp`
  - `src/kernel/Kernel.cpp`
  - `src/shell/Shell.hpp`
  - `src/shell/Shell.cpp`
  - `docs/phases/PHASE-01-process-control.md`
  - `docs/PROGRESS.md`
  - `README.md`
- Acceptance:
  - `run editor 5` creates PID 1
  - `ps` lists the process table
  - `kill 1` marks the process as terminated
  - invalid `kill` usage prints a clear error
- Commit message:
  - `Implement process control baseline with run ps kill`

## Phase 2: System Call Interface Layer

- Goal:
  Introduce the simulator equivalent of xv6's system call boundary so shell code no longer talks to kernel internals directly.
- xv6 mapping:
  Chapter 2 (user/supervisor boundary), Chapter 4 (system calls).
- Features:
  - Add a thin syscall/service facade
  - Route `run`, `ps`, `kill` through this layer
- Main data structures:
  - `Kernel`
  - `SystemCallInterface` or `OSInterface`
- Methods:
  - `sysRun(...)`
  - `sysPs()`
  - `sysKill(...)`
  - `sysGetTime()`
- Files:
  - new `src/syscalls/` or `src/system/` files
  - `src/shell/Shell.hpp`
  - `src/shell/Shell.cpp`
  - `docs/phases/PHASE-02-system-call-interface.md`
  - `docs/PROGRESS.md`
  - `README.md`
- Acceptance:
  - shell behavior is unchanged
  - shell owns the interface layer, not the process table
- Commit message:
  - `Add system call interface layer for shell commands`

## Phase 3: System Time And Step Skeleton

- Goal:
  Add a simulated clock and the smallest execution step model.
- xv6 mapping:
  Chapter 5 (timer interrupts), Chapter 7 (process progression).
- Features:
  - `step`
  - kernel time advancement
  - minimal process state transition rules
- Main data structures:
  - `systemTime`
  - process table
- Methods:
  - `Kernel::step()`
  - `Kernel::advanceTime()` or equivalent
  - `sysStep()`
- Files:
  - `src/kernel/Kernel.*`
  - syscall/interface layer
  - `src/shell/Shell.cpp`
  - `docs/phases/PHASE-03-system-step.md`
  - `docs/PROGRESS.md`
  - `README.md`
- Acceptance:
  - `step` advances time
  - at least one visible state or remaining-time change occurs
- Commit message:
  - `Add system step skeleton and simulated time`

## Phase 4: FCFS Scheduler Baseline

- Goal:
  Add the first real scheduling policy.
- xv6 mapping:
  Chapter 7 (scheduling and context switching concepts).
- Features:
  - ready process selection using FCFS
  - running process chosen from process table
- Main data structures:
  - ready queue view or ordered process scan
  - `Scheduler` / `FCFSScheduler`
- Methods:
  - scheduler selection method
  - kernel integration for choosing the running process
- Files:
  - `src/schedulers/Scheduler.hpp`
  - `src/schedulers/FCFSScheduler.*`
  - `src/kernel/Kernel.*`
  - `docs/phases/PHASE-04-fcfs.md`
  - `docs/PROGRESS.md`
  - `README.md`
- Acceptance:
  - multiple processes run in FCFS order under repeated `step`
- Commit message:
  - `Implement FCFS scheduling baseline`

## Phase 5: Round Robin And Metrics

- Goal:
  Add time-sliced scheduling and beginner-friendly scheduling metrics.
- xv6 mapping:
  Chapter 7 (scheduling), adapted with explicit simulator metrics.
- Features:
  - configurable quantum
  - round robin rotation
  - waiting time / turnaround time
  - simple Gantt-style terminal output
- Main data structures:
  - ready queue
  - execution history
- Methods:
  - `setScheduler(...)`
  - RR scheduler tick behavior
  - metrics aggregation helpers
- Files:
  - `src/schedulers/`
  - `src/kernel/Kernel.*`
  - `src/core/Simulation.*`
  - `docs/phases/PHASE-05-round-robin-metrics.md`
  - `docs/PROGRESS.md`
  - `README.md`
- Acceptance:
  - repeated `step` shows RR rotation
  - metrics print correctly for a small example
- Commit message:
  - `Add round robin scheduling and basic metrics`

## Phase 6: Memory Simulation Basics

- Goal:
  Turn xv6 memory ideas into a simulator-friendly memory manager.
- xv6 mapping:
  Chapter 3 (address spaces and memory allocation).
- Features:
  - total / used / free memory
  - per-process memory usage
  - `mem` command
  - optional first page-allocation model
- Main data structures:
  - memory manager state
  - per-process memory accounting
- Methods:
  - allocate/free helpers
  - `sysMemoryInfo()`
- Files:
  - new `src/memory/`
  - `src/kernel/Kernel.*`
  - syscall/interface layer
  - `src/shell/Shell.cpp`
  - `docs/phases/PHASE-06-memory-basics.md`
  - `docs/PROGRESS.md`
  - `README.md`
- Acceptance:
  - `mem` reports stable totals
  - creating processes affects memory accounting when enabled
- Commit message:
  - `Add basic memory manager and mem command`

## Phase 7: Virtual File System Basics

- Goal:
  Add the first simulator file system layer inspired by xv6's file and inode chapters.
- xv6 mapping:
  Chapter 1 (file descriptors overview), Chapter 8 (file system).
- Features:
  - `ls`
  - `touch <filename>`
  - `cat <filename>`
  - `write <filename> <content>`
  - `rm <filename>`
- Main data structures:
  - file table / directory map
  - virtual file content storage
- Methods:
  - create/read/write/delete/list helpers
  - syscall/interface methods for file operations
- Files:
  - new `src/fs/`
  - `src/kernel/Kernel.*`
  - syscall/interface layer
  - `src/shell/Shell.cpp`
  - `docs/phases/PHASE-07-virtual-filesystem.md`
  - `docs/PROGRESS.md`
  - `README.md`
- Acceptance:
  - files can be created, listed, written, and read back in memory
- Commit message:
  - `Add virtual file system baseline`

## Phase 8: I/O Blocking And Wakeup

- Goal:
  Simulate waiting processes and later wakeup behavior.
- xv6 mapping:
  Chapter 5 (device interaction), Chapter 7 (sleep and wakeup, pipes).
- Features:
  - simple blocking I/O task model
  - Waiting -> Ready transitions after a delay
  - optional event queue
- Main data structures:
  - waiting list
  - I/O event queue
- Methods:
  - block/unblock helpers
  - step-time event completion
- Files:
  - new `src/io/`
  - `src/kernel/Kernel.*`
  - `src/core/Simulation.*`
  - `docs/phases/PHASE-08-io-blocking.md`
  - `docs/PROGRESS.md`
  - `README.md`
- Acceptance:
  - an I/O task visibly enters `Waiting` and later returns to `Ready`
- Commit message:
  - `Add I/O blocking simulation and wakeup flow`

## Phase 9: Visualization Layer

- Goal:
  Add optional SFML-based views after the terminal model is stable.
- xv6 mapping:
  Project-specific presentation layer, not a direct xv6 subsystem.
- Features:
  - process table view
  - ready queue view
  - Gantt chart view
  - memory / file tree panels
- Main data structures:
  - visualization state derived from kernel snapshots
- Methods:
  - render/update helpers only
- Files:
  - new `src/ui/` or `src/visualization/`
  - `README.md`
  - `docs/phases/PHASE-09-visualization.md`
  - `docs/PROGRESS.md`
- Acceptance:
  - terminal simulator still works
  - visualization reflects simulator state without owning it
- Commit message:
  - `Add optional SFML visualization scaffolding`
