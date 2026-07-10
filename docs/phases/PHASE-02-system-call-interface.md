# Phase 2 - System Call Interface Layer

## Goal

Insert a thin simulator syscall layer between `Shell` and `Kernel`.

This mirrors xv6's idea that user-facing code should request services through a boundary instead of owning kernel state directly.

## Added Data Structures

- `SystemCallInterface`
  - owns a `Kernel`
  - exposes a minimal shell-safe API

## Added Or Updated Methods

- `SystemCallInterface::sysRun(...)`
  - forwards process creation to the kernel
- `SystemCallInterface::sysKill(int pid)`
  - forwards termination requests to the kernel
- `SystemCallInterface::sysPs()`
  - provides read-only access to the process table
- `SystemCallInterface::sysGetTime()`
  - exposes current simulated time

## How It Works

The shell now owns `SystemCallInterface` instead of `Kernel`.
The interface layer owns `Kernel` and forwards shell requests to it.
This keeps the current behavior while preparing the codebase for more simulator services such as `step`, scheduler setup, memory queries, and file operations.

## Current Limitations

- The interface is still a thin forwarding layer
- No trap or privilege simulation
- No scheduler or time-step behavior yet
- No memory or file system calls yet

## Next Phase Connection

The next phase should add a system step skeleton so the simulator can advance time and begin moving beyond static process creation.
