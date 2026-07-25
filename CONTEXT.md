# WUWAOS

WUWAOS is a terminal-first, Linux-like OS simulator. It is a normal C++ application that exposes a simulated kernel through shell commands; it is not a bootable operating system.

## Language

**WUWAOS**:
The project name for the OS simulator and algorithm visualizer.
_Avoid_: real OS, bootable OS

**terminal-first**:
The development direction where the shell-based simulator is built and verified before any graphical visualization.
_Avoid_: GUI-first

**simulated kernel**:
The C++ object layer that owns simulated OS state such as processes, scheduling, memory, virtual files, I/O, and system time.
_Avoid_: host kernel, real kernel

**shell command**:
A user-facing command typed at the `wuwaos$` prompt to operate on simulated OS state.
_Avoid_: host shell command, PowerShell command

**system call interface**:
The boundary that converts shell requests into kernel operations without letting the shell modify kernel data directly.
_Avoid_: direct shell-to-kernel mutation

**process table**:
The simulated kernel's collection of processes and their states.
_Avoid_: host process list

**scheduler**:
The component that selects a ready simulated process to run according to a scheduling policy.
_Avoid_: OS thread scheduler

**system clock**:
The simulated time advanced by WUWAOS commands such as `step`.
_Avoid_: wall-clock time, real time

**virtual file system**:
The future simulated file storage exposed through commands such as `ls`, `touch`, and `cat`.
_Avoid_: host filesystem

**small step**:
A development slice that changes one behavior or one workflow boundary and can be built, run, and explained.
_Avoid_: large rewrite, broad milestone
