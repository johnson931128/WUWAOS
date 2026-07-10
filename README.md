# WUWAOS

## Project Overview

WUWAOS is a terminal-first, Linux-like OS simulator.

It is not a real bootable operating system.
Instead, it simulates operating system behavior inside a normal C++ application so the project can be built step by step and inspected more easily.

## Current Features

- C++17 / CMake project structure
- Minimal terminal shell loop
- Basic `Process` model
- Basic Kernel skeleton with process table
- `help` / `clear` / `exit` commands
- `run <programName> <burstTime>` / `ps` / `kill <pid>` commands

Not implemented yet:

- System call interface layer
- Scheduler
- Memory / file system / I/O subsystems
- Simulated time step
- SFML visualization

## Build / Run

```powershell
cmake -S . -B build
cmake --build build
.\build\os_simulator.exe
```

## Example Usage

```text
wuwaos$ help
wuwaos$ clear
wuwaos$ run editor 5
wuwaos$ ps
wuwaos$ kill 1
wuwaos$ exit
```

## Roadmap

- System call interface layer
- Expand Kernel simulator
- Simulated time step
- FCFS / Round Robin scheduling
- Memory manager
- Virtual file system
- Optional SFML visualization
