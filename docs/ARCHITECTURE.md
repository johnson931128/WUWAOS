# WUWAOS Architecture

## 1. Project Identity

WUWAOS is a terminal-first, Linux-like operating system simulator.

It is not a real bootable operating system and does not replace the host operating system.
Instead, it simulates core OS concepts inside a normal C++ application.

The goal of WUWAOS is to provide an interactive environment where users can operate a small simulated OS through shell-like commands and observe how the simulated kernel manages processes, scheduling, memory, file systems, and I/O.

## 2. Long-Term Vision

The final version of WUWAOS should feel like using a tiny Linux-like system.

Example commands:

```text
wuwaos$ help
wuwaos$ ps
wuwaos$ run editor
wuwaos$ run counter
wuwaos$ kill 2
wuwaos$ sched fcfs
wuwaos$ sched rr 3
wuwaos$ step
wuwaos$ mem
wuwaos$ ls
wuwaos$ touch note.txt
wuwaos$ cat note.txt
wuwaos$ exit
```

These commands do not operate on the real machine directly.
They operate on the internal simulated OS state managed by WUWAOS.

## 3. High-Level Architecture

WUWAOS is divided into four major layers:

```text
+--------------------------------------------------+
| User Layer                                       |
| - Shell commands                                 |
| - Built-in simulated applications                |
+--------------------------------------------------+
                    |
                    v
+--------------------------------------------------+
| System Call Interface                            |
| - Converts user-level requests into kernel calls |
+--------------------------------------------------+
                    |
                    v
+--------------------------------------------------+
| Kernel Simulator                                 |
| - Process manager                                |
| - CPU scheduler                                  |
| - Memory manager                                 |
| - Virtual file system                            |
| - I/O manager                                    |
| - System clock                                   |
+--------------------------------------------------+
                    |
                    v
+--------------------------------------------------+
| Simulation Core                                  |
| - Data structures                                |
| - Algorithms                                     |
| - State transitions                              |
| - Logs and metrics                               |
+--------------------------------------------------+
```

## 4. User Layer

The user layer is the visible part of WUWAOS.

At the beginning, this layer is a terminal shell.
Later, it may be connected to a graphical interface.

Main responsibilities:

- Read user commands.
- Parse command arguments.
- Show command output.
- Provide a Linux-like operating experience.
- Allow users to interact with the simulated OS.

Example commands:

```text
help
exit
clear
ps
run <program>
kill <pid>
step
sched <algorithm>
mem
ls
touch <filename>
cat <filename>
```

## 5. System Call Interface

The system call interface is the boundary between user space and the simulated kernel.

The shell should not directly modify kernel data structures.
Instead, it should call clearly defined syscall-like functions.

Example conceptual calls:

```text
sys_run(programName)
sys_kill(pid)
sys_ps()
sys_step()
sys_set_scheduler(algorithm)
sys_memory_info()
sys_create_file(filename)
sys_list_files()
```

This layer helps WUWAOS keep a clear separation between user operations and kernel behavior.

## 6. Kernel Simulator

The kernel simulator is the core of WUWAOS.

It owns and controls the global simulated OS state.

Main responsibilities:

- Maintain system time.
- Manage the process table.
- Manage process state transitions.
- Select the next process through the scheduler.
- Track memory usage.
- Manage virtual files.
- Handle simulated I/O events.
- Provide syscall services to the user layer.

The kernel simulator should be the only layer allowed to directly coordinate OS subsystems.

## 7. Process Management

The process manager handles simulated processes.

Each process should contain basic information such as:

```text
pid
name
state
arrivalTime
burstTime
remainingTime
priority
memoryUsage
```

Possible process states:

```text
New
Ready
Running
Waiting
Terminated
```

The process manager is responsible for:

- Creating processes.
- Killing processes.
- Updating process states.
- Maintaining the process table.
- Providing process information to commands such as `ps`.

## 8. CPU Scheduling

The scheduler decides which ready process should run next.

Initial scheduling algorithms:

```text
FCFS
Round Robin
```

Future scheduling algorithms:

```text
SJF
SRTF
Priority Scheduling
```

The scheduler should not own the entire system state.
It should only decide which process should run based on the ready queue and scheduling policy.

## 9. Memory Management

The memory manager simulates memory usage.

Early version:

```text
total memory
used memory
free memory
memory usage per process
```

Future version:

```text
virtual memory
page table
page frames
page replacement
page fault
```

Possible page replacement algorithms:

```text
FIFO
LRU
Clock / Second Chance
Optimal
```

## 10. Virtual File System

The virtual file system simulates basic file operations.

Early commands:

```text
ls
touch <filename>
cat <filename>
write <filename> <content>
rm <filename>
```

Early version may store files only in memory.
Persistent disk storage is not required at the beginning.

Main responsibilities:

- Maintain a virtual directory.
- Create files.
- Read files.
- Write file contents.
- Delete files.

## 11. I/O Simulation

The I/O manager simulates blocking operations.

Example behavior:

```text
wuwaos$ run io-task
wuwaos$ step
Process 1 requests I/O and enters Waiting state.
wuwaos$ step
I/O completes. Process 1 returns to Ready state.
```

This allows WUWAOS to demonstrate process transitions between Running, Waiting, and Ready.

## 12. System Clock

WUWAOS should use a simulated clock.

The clock does not need to match real-world time.

Example:

```text
wuwaos$ step
Time advances from 0 to 1.
```

The `step` command is important because it allows users to observe the OS state one tick at a time.

Future version may support:

```text
play
pause
reset
```

## 13. Development Strategy

WUWAOS should be developed step by step.

The project should start from a terminal-based version before adding graphics.

Recommended order:

```text
Phase 1: Shell loop
- help
- clear
- exit

Phase 2: Process model
- Process class
- ProcessState
- process table

Phase 3: Basic process commands
- run <program>
- ps
- kill <pid>

Phase 4: Simulated time
- step
- system clock
- process execution tick

Phase 5: CPU scheduling
- FCFS
- Round Robin

Phase 6: Memory management
- total memory
- process memory usage
- mem command

Phase 7: Virtual file system
- ls
- touch
- cat
- write
- rm

Phase 8: I/O simulation
- Waiting state
- I/O completion event

Phase 9: Optional visualization
- SFML window
- process table
- ready queue
- Gantt chart
- memory view
- file tree
```

## 14. Design Principles

WUWAOS should prioritize clarity over complexity.

Important rules:

```text
Small steps.
Readable code.
Clear module boundaries.
No large rewrites without reason.
Terminal version first.
Visualization later.
Every feature must be verifiable.
```

The purpose of WUWAOS is not only to produce working code, but also to make operating system concepts easier to understand and inspect.
