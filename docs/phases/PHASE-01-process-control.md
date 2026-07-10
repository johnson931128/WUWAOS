# Phase 1 - Process Control Baseline

## Goal

Finish the first stable process-control slice:

- create simulated processes with `run`
- list them with `ps`
- terminate them with `kill`

This phase adapts xv6's early process/interface ideas into a small terminal-first simulator milestone.

## Added Data Structures

- Existing `Process`
- Existing kernel-owned `std::vector<Process>` process table

## Added Or Updated Methods

- `Kernel::createProcess(...)`
  - creates and stores a new simulated process
- `Kernel::killProcess(int pid)`
  - marks a process as terminated by pid
- `Kernel::getProcesses()`
  - exposes the process table for read-only display
- `Shell::handleRunCommand(...)`
  - parses and validates `run`
- `Shell::handlePsCommand()`
  - prints a simple process table
- `Shell::handleKillCommand(...)`
  - parses and validates `kill`

## How It Works

The shell parses commands and delegates all process ownership and state mutation to `Kernel`.
The kernel owns `systemTime`, `nextPid`, and the process table.
The shell never stores or edits the `Process` vector directly.

## Current Limitations

- No scheduler
- No `step`
- No waiting/running transitions beyond explicit termination
- No system call interface layer yet
- No memory or file system behavior yet

## Next Phase Connection

The next phase should insert a system call interface layer between `Shell` and `Kernel` so the command surface is stable before the simulator grows further.
