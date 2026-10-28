#include "process.h"
#include "console.h"
#include "syscall_numbers.h"
#include "vm.h"
#include "io.h"
#include "scheduler.h"
#include "timer.h"
#include "clock.h"
#include "workstate.h"
#include "fs.h"
#include "path.h"
#include <limits.h>

static Process processes[PROCESS_LIMIT];
static unsigned char kernel_stacks[PROCESS_LIMIT][16 * 1024]
    __attribute__((aligned(16)));
static int current_index = -1;
static long next_pid = 1;
static int trace_enabled = 1;
static unsigned char scheduler_stack[16 * 1024] __attribute__((aligned(16)));
static KernelContext scheduler_context, boot_context;

static _Noreturn void first_run(void) {
    user_enter(process_current()->frame);
}

static void suspend_current(void) {
    Process *process = process_current();
    uintptr_t status;
    __asm__ volatile("csrr %0, sstatus" : "=r"(status));
    if ((status & (1UL << 1)) || process->state == PROCESS_RUNNING) {
        console_puts("[context] Switch requires SIE=0 and a non-Running process\n");
        kernel_idle();
    }
    timer_stop();
    workstate_pause(process);
    context_switch(&process->context, &scheduler_context);
    /* A later dispatch restores this very C call stack and its local variables. */
}

const char *process_state_name(ProcessState state) {
    static const char *names[] = {
        "Unused", "New", "Ready", "Running", "Waiting", "Zombie"
    };
    return names[state];
}

static void transition(Process *process, ProcessState next, const char *reason) {
    const ProcessState previous = process->state;
    if (previous == PROCESS_READY && next != PROCESS_READY) {
        scheduler_remove(process);
    }
    process->state = next;
    if (next == PROCESS_READY) {
        scheduler_enqueue(process);
    }
    /* Keep the interactive prompt readable; inspect the shell through pcb. */
    if (!trace_enabled || process->pid == 1 || process->program == PROGRAM_FAN) {
        return;
    }
    console_puts("[process] PID=");
    console_number(process->pid);
    console_putc(' ');
    console_puts(process_state_name(previous));
    console_puts(" -> ");
    console_puts(process_state_name(next));
    console_puts(" (");
    console_puts(reason);
    console_puts(")\n");
}

Process *process_current(void) {
    return &processes[current_index];
}

Process *process_slot(int slot) {
    return &processes[slot];
}

static Process *find_pid(long pid) {
    for (int i = 0; i < PROCESS_LIMIT; ++i) {
        if (processes[i].state != PROCESS_UNUSED && processes[i].pid == pid) {
            return &processes[i];
        }
    }
    return 0;
}

long process_chdir(const char *path) {
    Process *process = process_current();
    char absolute[WUWA_PATH_MAX], canonical[WUWA_PATH_MAX];
    if (path[0] == '-' && path[1] == 0) {
        if (!process->previous_cwd[0]) return -2;
        path = process->previous_cwd;
    }
    int result = path_absolute(process->cwd, path, absolute);
    if (result < 0) return result;
    result = (int)fs_is_directory(absolute);
    if (result < 0) return result;
    path_canonical(absolute, canonical);
    size_t i = 0;
    do { process->previous_cwd[i] = process->cwd[i]; } while (process->cwd[i++]);
    i = 0;
    do { process->cwd[i] = canonical[i]; } while (canonical[i++]);
    return 0;
}

static long create_process(long program, long parent_pid, int publish) {
    int slot = -1;
    for (int i = 0; i < PROCESS_LIMIT; ++i) {
        if (processes[i].state == PROCESS_UNUSED) {
            slot = i;
            break;
        }
    }
    if (slot == -1 || next_pid == LONG_MAX) {
        return ERR_NO_SLOT;
    }

    Process *process = &processes[slot];
    volatile unsigned char *bytes = (volatile unsigned char *)process;
    for (size_t i = 0; i < sizeof(*process); ++i) {
        bytes[i] = 0;
    }
    process->pid = next_pid++;
    process->parent_pid = parent_pid;
    Process *parent = find_pid(parent_pid);
    if (parent) {
        for (size_t i = 0; i < WUWA_PATH_MAX; ++i) {
            process->cwd[i] = parent->cwd[i];
            process->previous_cwd[i] = parent->previous_cwd[i];
        }
    } else { process->cwd[0] = '/'; process->cwd[1] = 0; }
    process->program = program;
    process->input_pipe = -1;
    process->output_pipe = -1;
    transition(process, PROCESS_NEW, "create");
    process->page_table = vm_create(program);
    if (process->page_table == 0) {
        transition(process, PROCESS_UNUSED, "out of memory");
        return ERR_NO_MEMORY;
    }
    for (size_t i = 0; i < sizeof(kernel_stacks[slot]); ++i) {
        ((volatile unsigned char *)kernel_stacks[slot])[i] = 0;
    }
    process->kernel_stack_top = (uintptr_t)(kernel_stacks[slot] + sizeof(kernel_stacks[slot]));
    process->user_stack_top = USER_STACK_TOP;
    process->frame = (TrapFrame *)(process->kernel_stack_top - TF_SIZE);
    process->context.ra = (uintptr_t)first_run;
    process->context.sp = (uintptr_t)process->frame; /* 16-byte aligned, below frame. */
    process->frame->registers[2] = USER_STACK_TOP;
    process->frame->registers[10] = (uint64_t)program;
    process->frame->sepc = USER_ENTRY;
    uintptr_t status;
    __asm__ volatile("csrr %0, sstatus" : "=r"(status));
    process->frame->sstatus = (status &
        ~((1UL << 8) | (1UL << 5) | (1UL << 1) | (1UL << 18) | (1UL << 19) |
          (3UL << 13) | (3UL << 9))) | (1UL << 5); /* SPIE: sret enables S interrupts. */
    if (publish) {
        transition(process, PROCESS_READY, "initialized");
    }
    return process->pid;
}

void process_init(void) {
    scheduler_init();
    workstate_init();
    if (create_process(0, 0, 1) != 1) {
        console_puts("[process] Cannot create init\n");
        kernel_idle();
    }
}

long process_spawn(long program) {
    if (program != PROGRAM_WORKER && program != PROGRAM_QUICK) {
        return ERR_INVALID;
    }
    return create_process(program, process_current()->pid, 1);
}

long process_fork(void) {
    Process *parent = process_current();
    const long pid = create_process(parent->program, parent->pid, 0);
    if (pid < 0) {
        return pid;
    }
    Process *child = find_pid(pid);
    const int copy_error = vm_clone(child->page_table, parent->page_table);
    if (copy_error < 0) {
        vm_destroy(child->page_table);
        child->page_table = 0;
        transition(child, PROCESS_UNUSED, "fork copy failed");
        return copy_error;
    }
    volatile unsigned char *to = (volatile unsigned char *)child->frame;
    const unsigned char *from = (const unsigned char *)parent->frame;
    for (size_t i = 0; i < sizeof(TrapFrame); ++i) {
        to[i] = from[i];
    }
    child->frame->registers[10] = 0;
    io_inherit(child, parent);
    transition(child, PROCESS_READY, "fork copied user state");
    return pid;
}

long process_exec(long program, const char *argument) {
    if (program < PROGRAM_WORKER || program > PROGRAM_FAN ||
        (program == PROGRAM_FAN && (argument == 0 || argument[0] != '/'))) {
        return ERR_INVALID; /* Preserve the current image on failure. */
    }
    Process *process = process_current();
    const uintptr_t root = vm_create(program);
    if (root == 0) return ERR_NO_MEMORY;
    uintptr_t stack = USER_STACK_TOP;
    if (program == PROGRAM_FAN) {
        stack -= 128; /* Keep the path above the downward-growing call stack. */
        size_t length = 0;
        while (argument[length]) ++length;
        const int error = user_copy_to(root, stack, argument, length + 1);
        if (error < 0) { vm_destroy(root); return error; }
    }
    (void)io_console_mode(0); /* Successful exec resets raw input ownership. */
    const uintptr_t old_root = process->page_table;
    const uint64_t status = process->frame->sstatus;
    volatile unsigned char *bytes = (volatile unsigned char *)process->frame;
    for (size_t i = 0; i < sizeof(TrapFrame); ++i) {
        bytes[i] = 0;
    }
    process->frame->registers[2] = stack;
    process->frame->registers[10] = (uint64_t)program;
    process->frame->registers[11] = program == PROGRAM_FAN ? stack : 0;
    process->frame->sepc = USER_ENTRY;
    process->frame->sstatus = status;
    process->program = program;
    process->page_table = root;
    process->wait_reason = WAIT_NONE;
    process->channel = process->wait_pid = 0;
    process->wait_status = 0;
    process->io_operation = IO_NONE;
    process->io_buffer = process->io_length = process->io_count = 0;
    process->io_overflow = 0;
    vm_enable(root);
    vm_destroy(old_root);
    console_puts("[exec] PID=");
    console_number(process->pid);
    console_puts(" retained; loaded app image, program=");
    console_number(program);
    console_putc('\n');
    return 0;
}

long process_yield(void) {
    transition(process_current(), PROCESS_READY, "yield");
    suspend_current();
    return 0;
}

void process_preempt(void) {
    Process *process = process_current();
    ++process->preemptions;
    transition(process, PROCESS_READY, "time slice expired");
    suspend_current();
}

long process_sleep(long channel) {
    if (channel <= 0) {
        return ERR_INVALID;
    }
    Process *process = process_current();
    process->wait_reason = WAIT_CHANNEL;
    process->channel = channel;
    transition(process, PROCESS_WAITING, "sleep channel");
    suspend_current();
    return 0;
}

long process_sleep_ms(long milliseconds) {
    if (milliseconds < 1 || milliseconds > 60000) return ERR_INVALID;
    Process *process = process_current();
    process->wake_at = clock_ticks() + clock_ms_ticks((unsigned long)milliseconds);
    process->wait_reason = WAIT_TIMER;
    transition(process, PROCESS_WAITING, "timed sleep");
    suspend_current();
    return 0;
}

static int wake_timed(void) {
    const uint64_t now = clock_ticks();
    int pending = 0;
    for (int i = 0; i < PROCESS_LIMIT; ++i) {
        Process *process = &processes[i];
        if (process->state != PROCESS_WAITING || process->wait_reason != WAIT_TIMER) continue;
        if ((int64_t)(now - process->wake_at) >= 0) {
            process->wait_reason = WAIT_NONE;
            process->wake_at = 0;
            transition(process, PROCESS_READY, "timer wakeup");
        } else pending = 1;
    }
    return pending;
}

long process_wakeup(long channel) {
    if (channel <= 0) {
        return ERR_INVALID;
    }
    long count = 0;
    for (int i = 0; i < PROCESS_LIMIT; ++i) {
        Process *process = &processes[i];
        if (process->state == PROCESS_WAITING &&
            process->wait_reason == WAIT_CHANNEL && process->channel == channel) {
            process->wait_reason = WAIT_NONE;
            process->channel = 0;
            transition(process, PROCESS_READY, "wakeup channel");
            ++count;
        }
    }
    return count;
}

static void reap(Process *process) {
    /* The caller must be on another root; kernel stacks remain static. */
    vm_destroy(process->page_table);
    process->page_table = 0;
    transition(process, PROCESS_UNUSED, "reaped");
}

static long collect_child(Process *parent, Process *child, uintptr_t status) {
    if (status != 0) {
        if (!user_writable(parent->page_table, status, sizeof(long))) {
            return ERR_ADDRESS;
        }
        const int error = user_write_long(parent->page_table, status, child->exit_status);
        if (error < 0) return error;
    }
    const long pid = child->pid;
    console_puts("[wait] collected PID=");
    console_number(pid);
    console_puts(" status=");
    console_number(child->exit_status);
    console_putc('\n');
    reap(child);
    return pid;
}

long process_wait(long pid, uintptr_t status_address) {
    Process *parent = process_current();
    Process *child = find_pid(pid);
    if (child == 0 || child->parent_pid != parent->pid) {
        return ERR_NO_CHILD;
    }
    if (status_address != 0 &&
        !user_writable(parent->page_table, status_address, sizeof(long))) {
        return ERR_ADDRESS;
    }
    if (child->state == PROCESS_ZOMBIE) {
        return collect_child(parent, child, status_address);
    }
    parent->wait_reason = WAIT_CHILD;
    parent->wait_pid = pid;
    parent->wait_status = status_address;
    transition(parent, PROCESS_WAITING, "wait child");
    suspend_current();
    /* Resume inside wait, on this parent's original kernel stack. */
    parent->wait_reason = WAIT_NONE;
    parent->wait_pid = 0;
    parent->wait_status = 0;
    child = find_pid(pid);
    return child != 0 && child->parent_pid == parent->pid && child->state == PROCESS_ZOMBIE
        ? collect_child(parent, child, status_address) : ERR_NO_CHILD;
}

static void terminate(Process *process, long status) {
    io_close_process(process);
    process->io_operation = IO_NONE;
    process->wait_reason = WAIT_NONE;
    process->channel = 0;
    process->wait_pid = 0;
    process->wait_status = 0;
    process->exit_status = status;
    transition(process, PROCESS_ZOMBIE, "exit");
    console_puts("[exit] PID=");
    console_number(process->pid);
    console_puts(" status=");
    console_number(status);
    console_putc('\n');

    Process *init = find_pid(1);
    const long adopter = init != 0 && init != process &&
        init->state != PROCESS_ZOMBIE ? 1 : 0;
    for (int i = 0; i < PROCESS_LIMIT; ++i) {
        Process *child = &processes[i];
        if (child->state != PROCESS_UNUSED && child->parent_pid == process->pid) {
            child->parent_pid = adopter;
        }
    }
    Process *parent = find_pid(process->parent_pid);
    if (parent != 0 && parent->state == PROCESS_WAITING &&
        parent->wait_reason == WAIT_CHILD && parent->wait_pid == process->pid) {
        transition(parent, PROCESS_READY, "child exited");
    }
}

_Noreturn void process_exit(long status) {
    terminate(process_current(), status);
    suspend_current();
    console_puts("[context] Zombie was dispatched\n");
    kernel_idle();
}

long process_kill(long pid) {
    Process *target = find_pid(pid);
    if (target == 0 || target == process_current() || target->pid == 1 ||
        target->parent_pid != process_current()->pid || target->state == PROCESS_ZOMBIE) {
        return ERR_INVALID;
    }
    terminate(target, -9);
    return 0;
}

long process_block_io(IoOperation operation, uintptr_t buffer, size_t length) {
    Process *process = process_current();
    process->io_operation = operation;
    process->io_buffer = buffer;
    process->io_length = length;
    process->io_count = 0;
    process->io_overflow = 0;
    process->io_result = 0;
    process->wait_reason = WAIT_IO;
    transition(process, PROCESS_WAITING, (operation == IO_CONSOLE_READ || operation == IO_CONSOLE_RAW) ? "console read" :
               operation == IO_PIPE_READ ? "pipe read" : "pipe write");
    suspend_current();
    return process->io_result;
}

void process_complete_io(Process *process, long result) {
    process->io_result = result;
    process->io_operation = IO_NONE;
    process->wait_reason = WAIT_NONE;
    transition(process, PROCESS_READY, "I/O ready");
}

static const char *program_name(long program) {
    static const char *names[] = { "shell", "worker", "quick", "writer", "reader", "counter", "fan" };
    return names[program];
}

const char *process_name(const Process *process) { return program_name(process->program); }

static const char *waiting_for(const Process *process) {
    if (process->state != PROCESS_WAITING) {
        return "-";
    }
    if (process->wait_reason == WAIT_CHILD) {
        return "child";
    }
    if (process->wait_reason == WAIT_CHANNEL) {
        return "channel";
    }
    if (process->wait_reason == WAIT_TIMER) return "timer";
    return (process->io_operation == IO_CONSOLE_READ || process->io_operation == IO_CONSOLE_RAW) ? "console" :
           process->io_operation == IO_PIPE_READ ? "pipe-read" : "pipe-write";
}

long process_list(void) {
    console_puts("PID PPID NAME STATE WAIT\n");
    long count = 0;
    for (int i = 0; i < PROCESS_LIMIT; ++i) {
        const Process *process = &processes[i];
        if (process->state == PROCESS_UNUSED) {
            continue;
        }
        ++count;
        console_number(process->pid);
        console_putc(' ');
        console_number(process->parent_pid);
        console_putc(' ');
        console_puts(program_name(process->program));
        console_putc(' ');
        console_puts(process_state_name(process->state));
        console_putc(' ');
        console_puts(waiting_for(process));
        console_putc('\n');
    }
    console_puts("Free PCB slots: ");
    console_number(PROCESS_LIMIT - count);
    console_putc('\n');
    scheduler_print_ready();
    page_print_stats();
    return count;
}

long process_inspect(long pid) {
    const Process *process = find_pid(pid);
    if (process == 0) {
        return ERR_INVALID;
    }
    console_puts("PCB PID=");
    console_number(pid);
    console_puts(" PPID=");
    console_number(process->parent_pid);
    console_puts(" name=");
    console_puts(program_name(process->program));
    console_puts(" state=");
    console_puts(process_state_name(process->state));
    console_puts(" ready_queued=");
    console_number(process->ready_queued);
    console_puts("\ncwd=");
    console_puts(process->cwd);
    console_puts("\ndispatches=");
    console_number((long)process->dispatches);
    console_puts(" preemptions=");
    console_number((long)process->preemptions);
    console_puts("\npage_table=");
    console_hex(process->page_table);
    console_puts(" kernel_stack_top=");
    console_hex(process->kernel_stack_top);
    console_puts("\nkernel_context_sp=");
    console_hex(process->context.sp);
    console_puts(" kernel_context_ra=");
    console_hex(process->context.ra);
    console_puts(" (saved at last switch; stale while Running)");
    console_puts(" user_stack_top=");
    console_hex(process->user_stack_top);
    console_puts("\nsepc=");
    console_hex(process->frame->sepc);
    console_puts(" saved_sp=");
    console_hex(process->frame->registers[2]);
    console_puts("\nwait=");
    console_puts(waiting_for(process));
    console_puts(" channel=");
    console_number(process->channel);
    console_puts(" child=");
    console_number(process->wait_pid);
    console_puts(" exit_status=");
    console_number(process->exit_status);
    console_puts("\nstdin_pipe=");
    console_number(process->input_pipe);
    console_puts(" stdout_pipe=");
    console_number(process->output_pipe);
    console_puts("\nio_buffer=");
    console_hex(process->io_buffer);
    console_puts(" length=");
    console_number((long)process->io_length);
    console_puts(" completed=");
    console_number((long)process->io_count);
    console_putc('\n');
    vm_print_space(process->page_table);
    return 0;
}

long process_trace(long enabled) {
    if (enabled != 0 && enabled != 1) {
        return ERR_INVALID;
    }
    trace_enabled = (int)enabled;
    return 0;
}

long process_pipeline(uintptr_t result_address) {
    Process *parent = process_current();
    if (!user_writable(parent->page_table, result_address, 2 * sizeof(long))) {
        return ERR_ADDRESS;
    }
    int free_slots = 0;
    for (int i = 0; i < PROCESS_LIMIT; ++i) {
        free_slots += processes[i].state == PROCESS_UNUSED;
    }
    if (free_slots < 2 || next_pid > LONG_MAX - 2) {
        return ERR_NO_SLOT;
    }
    const int pipe = io_pipe_create();
    if (pipe < 0) {
        return ERR_NO_SLOT;
    }
    const long writer = create_process(PROGRAM_WRITER, parent->pid, 1);
    const long reader = create_process(PROGRAM_READER, parent->pid, 1);
    if (writer < 0 || reader < 0) {
        if (writer > 0) {
            reap(find_pid(writer));
        }
        if (reader > 0) {
            reap(find_pid(reader));
        }
        io_pipe_release(pipe);
        return writer < 0 ? writer : reader;
    }
    find_pid(writer)->output_pipe = pipe;
    find_pid(reader)->input_pipe = pipe;
    const long pids[2] = {writer, reader};
    const int error = user_copy_to(parent->page_table, result_address, pids, sizeof(pids));
    if (error < 0) {
        io_close_process(find_pid(writer));
        io_close_process(find_pid(reader));
        reap(find_pid(writer));
        reap(find_pid(reader));
        return error;
    }
    console_puts("[pipe] writer=");
    console_number(writer);
    console_puts(" reader=");
    console_number(reader);
    console_puts(" capacity=16\n");
    return 0;
}

TrapFrame *process_resume(void) {
    vm_use_kernel();
    io_poll();
    if (process_current()->state != PROCESS_RUNNING) suspend_current();
    vm_enable(process_current()->page_table);
    return process_current()->frame;
}

static _Noreturn void scheduler_loop(void) {
    for (;;) {
        /* Never reap a process while executing on its stack or its root. */
        vm_use_kernel();
        current_index = -1;
        const int timed_waiting = wake_timed();
        io_poll();
        for (int i = 0; i < PROCESS_LIMIT; ++i) {
            if (processes[i].state == PROCESS_ZOMBIE && processes[i].parent_pid == 0) {
                reap(&processes[i]);
            }
        }
        Process *next = scheduler_next();
        if (next != 0) {
            current_index = (int)(next - processes);
            transition(next, PROCESS_RUNNING, "schedule");
            ++next->dispatches;
            vm_enable(next->page_table);
            timer_start_slice(SCHEDULER_QUANTUM_MS);
            workstate_dispatch(next);
            context_switch(&scheduler_context, &next->context);
            continue;
        }

        if (io_console_waiting() || timed_waiting) {
            continue; /* Poll UART/deadlines while every process is blocked. */
        }
        int waiting = 0;
        for (int i = 0; i < PROCESS_LIMIT; ++i) {
            if (processes[i].state == PROCESS_WAITING) {
                ++waiting;
                console_puts("[scheduler] Waiting PID=");
                console_number(processes[i].pid);
                console_puts(" for ");
                console_puts(waiting_for(&processes[i]));
                console_putc('\n');
            }
        }
        console_puts(waiting ? "[scheduler] No Ready process; blocked without an event source\n"
                            : "[scheduler] All processes reaped; idle\n");
        kernel_idle();
    }
}

_Noreturn void process_start(void) {
    scheduler_context.ra = (uintptr_t)scheduler_loop;
    scheduler_context.sp = (uintptr_t)(scheduler_stack + sizeof(scheduler_stack));
    console_puts("[context] RV64 swtch ready; dedicated scheduler stack\n");
    context_switch(&boot_context, &scheduler_context);
    kernel_idle();
}
