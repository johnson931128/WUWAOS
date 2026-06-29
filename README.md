# OS-Simulator

OS-Simulator is an OS Simulator / OS Algorithm Visualizer project.

The long-term goal is to build the project step by step and use it to understand operating system concepts such as CPU scheduling, process states, memory management, page replacement, deadlock detection, and disk scheduling.

## Current Stage

The project is currently in the terminal-based CPU scheduling simulator bootstrap stage.

At this point, the project only provides:

- A minimal C++ / CMake project structure
- Placeholder source files for future simulator components
- A simple executable that confirms the bootstrap runs successfully

No scheduling logic has been implemented yet.

## Build

```powershell
cmake -S . -B build
cmake --build build
```

## Run

```powershell
.\build\os_simulator.exe
```
