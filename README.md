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
- System call interface layer
- FCFS scheduler baseline
- `help` / `clear` / `exit` commands
- `run <programName> <burstTime>` / `ps` / `kill <pid>` / `step` commands

Not implemented yet:

- Round Robin scheduling
- Waiting / turnaround metrics
- Memory / file system / I/O subsystems
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
wuwaos$ step
wuwaos$ ps
wuwaos$ kill 1
wuwaos$ exit
```

## Roadmap

- Expand Kernel simulator
- Round Robin scheduling
- Memory manager
- Virtual file system
- Optional SFML visualization
