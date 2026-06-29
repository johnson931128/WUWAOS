# AGENTS.md

## Project Goal

This project is an OS Simulator / OS Algorithm Visualizer.

The goal is to build the project step by step, starting from small and understandable features.
The project should help visualize operating system concepts such as CPU scheduling, process states, memory management, page replacement, deadlock detection, and disk scheduling.

The main purpose is not only to generate code, but also to improve understanding of C++, data structures, algorithms, and operating system design.

## Collaboration Rules

Before modifying code, explain the plan first.

For every task, follow this order:

1. Explain what the feature should do.
2. List the files that may need to be changed.
3. Describe the smallest reasonable implementation step.
4. Wait for confirmation if the change is large or unclear.
5. Modify only the necessary code.
6. After modifying, summarize the changes clearly.

## Coding Style

Keep the code simple and readable.

Prefer small classes and small functions.
Avoid large rewrites unless clearly necessary.
Do not introduce complicated architecture too early.

Use clear names for classes, functions, and variables.

Examples:

- `Process`
- `Scheduler`
- `FCFSScheduler`
- `Simulation`
- `ReadyQueue`
- `GanttChart`

## Learning Rule

Do not only provide finished code.

After every important change, explain:

1. What was changed.
2. Why it was changed.
3. How the new code works.
4. Which part is worth reading carefully.
5. What can be practiced manually next.

## Task Size Rule

Each task should be small.

Good task examples:

- Create the `Process` class.
- Implement FCFS scheduling.
- Print a terminal-based Gantt chart.
- Add one simulation tick.
- Show the ready queue.
- Add Round Robin time quantum.

Bad task examples:

- Build the whole OS simulator.
- Add all scheduling algorithms at once.
- Rewrite the entire project structure.
- Add GUI, scheduling, memory, and deadlock together.

## Project Progress Rule

After each completed task, update or suggest updates to the project notes.

The notes should include:

- What is completed.
- How to run the project.
- What is not finished yet.
- What the next small step should be.

## Testing Rule

Whenever possible, provide a simple way to verify the result.

For example:

- Build command.
- Run command.
- Expected terminal output.
- Small test input.
- Expected Gantt chart.
- Expected scheduling result.

## Current Development Direction

Start with a terminal-based CPU Scheduling Simulator.

Initial milestone:

1. Create a basic `Process` structure.
2. Implement FCFS scheduling.
3. Simulate time step by step.
4. Print the running process at each tick.
5. Print a simple Gantt chart.
6. Calculate waiting time and turnaround time.

After the terminal version is stable, add SFML visualization.
