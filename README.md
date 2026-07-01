# WUWAOS

## Project Overview

WUWAOS is a terminal-first, Linux-like OS simulator.

It is not a real bootable operating system.
Instead, it simulates operating system behavior inside a normal C++ application so the project can be built step by step and inspected more easily.

## Current Features

- C++17 / CMake project structure
- Minimal terminal shell loop
- `help` / `clear` / `exit` commands

Not implemented yet:

- Process model
- Kernel simulator
- Scheduler
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
wuwaos$ exit
```

## Roadmap

- Process model
- `run` / `ps` / `kill` commands
- Kernel simulator
- Simulated time step
- FCFS / Round Robin scheduling
- Memory manager
- Virtual file system
- Optional SFML visualization
