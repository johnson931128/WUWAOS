#include "kernel/proc/process.h"
#include "kernel/proc/terminal_service.h"
#include "kernel/proc/event_hooks.h"
#include "kernel/io/console.h"
#include "kernel/io/tty.h"
#include "syscall_numbers.h"
#include "kernel/mm/vm.h"
#include "kernel/io/io.h"
#include "kernel/proc/scheduler.h"
#include "arch/riscv/timer.h"
#include "arch/riscv/clock.h"
#include "kernel/proc/workstate.h"
#include "kernel/fs/fs.h"
#include "kernel/fs/path.h"
#include "kernel/ipc/sync.h"
#include "kernel/exec/elf.h"
#include "drivers/virtio/block.h"
#include "arch/riscv/irq.h"
#include <limits.h>

static Process processes[PROCESS_LIMIT];
#define PROCESS_KERNEL_STACK_SIZE (16UL * 1024UL)
#define KERNEL_STACK_REGION_SIZE (PAGE_SIZE + PROCESS_KERNEL_STACK_SIZE)
static ProcessPollCallback ui_poll_callback;
static ProcessEventActiveCallback ui_active_callback;
static unsigned poll_context_depth;
static int runtime_started;

void process_register_ui_events(ProcessPollCallback poll, ProcessEventActiveCallback active) {
    ui_poll_callback = poll;
    ui_active_callback = active;
}

static void process_poll_ui(void) {
    if (ui_poll_callback) ui_poll_callback();
}

static int process_ui_event_active(void) {
    return ui_active_callback && ui_active_callback();
}

static void process_poll_sources(void) {
    ++poll_context_depth;
    io_poll();
    block_poll();
    process_poll_ui();
    --poll_context_depth;
}

int process_runtime_started(void) { return runtime_started; }

int thread_wait_allowed(void) {
    Thread *thread = thread_current_or_null();
    return thread && thread->state == THREAD_RUNNING && !poll_context_depth && !irq_in_handler();
}

int thread_runtime_nonblocking(void) {
    return irq_in_handler() || poll_context_depth != 0 || (runtime_started && process_current_or_null() == 0);
}

void thread_gate_acquired(Thread *thread) {
    if (!thread) return;
    if (thread->internal_gate_depth == UINT_MAX) {
        console_puts("[gate] Thread gate depth overflow\n");
        kernel_idle();
    }
    ++thread->internal_gate_depth;
}

void thread_gate_released(Thread *thread) {
    if (!thread) return;
    if (!thread->internal_gate_depth) {
        console_puts("[gate] Thread gate depth underflow\n");
        kernel_idle();
    }
    --thread->internal_gate_depth;
}

int thread_cancel_defer_enter(void) {
    Thread *thread = thread_current_or_null();
    if (!thread) return 0;
    if (thread->cancel_defer_depth == UINT_MAX) return ERR_INVALID;
    ++thread->cancel_defer_depth;
    return 0;
}

int thread_cancel_defer_leave(void) {
    Thread *thread = thread_current_or_null();
    if (!thread) return 0;
    if (!thread->cancel_defer_depth) return ERR_INVALID;
    --thread->cancel_defer_depth;
    return 0;
}

static int thread_event_waiting(void) {
    for (int i = 0; i < THREAD_LIMIT; ++i)
        if (thread_slot(i)->event_wait.active && !thread_slot(i)->event_wait.completed) return 1;
    return 0;
}

static uint64_t thread_event_deadline(uint64_t fallback) {
    uint64_t deadline = fallback;
    for (int i = 0; i < THREAD_LIMIT; ++i) {
        const KernelEventWait *wait = &thread_slot(i)->event_wait;
        if (!wait->active || wait->completed || !wait->deadline) continue;
        if ((int64_t)(wait->deadline - deadline) < 0) deadline = wait->deadline;
    }
    return deadline;
}
static long next_pid = 1;
static int trace_enabled = 1;
static unsigned char scheduler_stack[KERNEL_STACK_REGION_SIZE] __attribute__((aligned(PAGE_SIZE)));
static KernelContext scheduler_context, boot_context;
static void thread_cancel_wait(Thread *thread);

static _Noreturn void first_run(void) {
    user_enter(thread_current()->frame);
}

static void suspend_current(void) {
    Thread *thread = thread_current();
    uintptr_t status;
    __asm__ volatile("csrr %0, sstatus" : "=r"(status));
    if ((status & (1UL << 1)) || thread->state == THREAD_RUNNING) {
        console_puts("[context] Switch requires SIE=0 and a non-Running thread\n");
        kernel_idle();
    }
    timer_stop();
    workstate_pause(thread);
    context_switch(&thread->context, &scheduler_context);
    /* A later dispatch restores this very C call stack and its local variables. */
}

const char *process_state_name(ProcessState state) {
    static const char *names[] = { "Unused", "New", "Live", "Dying", "Zombie" };
    return names[state];
}

const char *thread_state_name(ThreadState state) {
    static const char *names[] = { "Unused", "New", "Ready", "Running", "Waiting", "Zombie" };
    return names[state];
}

static void thread_transition(Thread *thread, ThreadState next, const char *reason) {
    const ThreadState previous = thread->state;
    if (previous == THREAD_READY && next != THREAD_READY) {
        scheduler_remove(thread);
    }
    thread->state = next;
    if (next == THREAD_READY) {
        scheduler_enqueue(thread);
    }
    /* Keep the interactive prompt readable; inspect the shell through pcb. */
    if (!trace_enabled || thread->process->pid == 1 || thread->process->program == PROGRAM_FAN ||
        thread->process->program == PROGRAM_WORKSTATE) {
        return;
    }
    console_puts("[thread] PID=");
    console_number(thread->process->pid);
    console_puts(" TID=");
    console_number(thread->tid);
    console_putc(' ');
    console_puts(thread_state_name(previous));
    console_puts(" -> ");
    console_puts(thread_state_name(next));
    console_puts(" (");
    console_puts(reason);
    console_puts(")\n");
}

Process *process_current(void) {
    return thread_current()->process;
}

Process *process_current_or_null(void) {
    Thread *thread = thread_current_or_null();
    return thread ? thread->process : 0;
}

int thread_kernel_stack_bounds(uintptr_t address, uintptr_t *bottom, uintptr_t *top,
                               int *scheduler) {
    if (!bottom || !top || !scheduler) return 0;
    if (thread_stack_guard_bounds(address, bottom, top)) {
        *scheduler = 0;
        return 1;
    }
    const uintptr_t guard = (uintptr_t)scheduler_stack;
    if ((address & ~(PAGE_SIZE - 1)) != guard) return 0;
    *bottom = guard + PAGE_SIZE;
    *top = *bottom + PROCESS_KERNEL_STACK_SIZE;
    *scheduler = 1;
    return 1;
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

static void discard_initial_thread(Process *process) {
    thread_release(process->thread);
    process->thread = 0;
    process->state = PROCESS_UNUSED;
}

static void publish_process(Process *process, const char *reason) {
    process->state = PROCESS_LIVE;
    thread_transition(process->thread, THREAD_READY, reason);
}

static void mark_zombie(Process *process, const char *reason) {
    thread_transition(process->thread, THREAD_ZOMBIE, reason);
    process->state = PROCESS_ZOMBIE;
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
    for (int fd = 0; fd < PROCESS_FD_LIMIT; ++fd) process->fd_table[fd] = -99;
    process->fd_table[0] = -1;
    process->fd_table[1] = -2;
    process->fd_table[2] = -3;
    process->state = PROCESS_NEW;
    process->thread = thread_allocate(process);
    if (!process->thread) { process->state = PROCESS_UNUSED; return ERR_NO_SLOT; }
    process->page_table = vm_create(program);
    if (process->page_table == 0) {
        discard_initial_thread(process);
        return ERR_NO_MEMORY;
    }
    process->thread->user_stack_top = USER_STACK_TOP;
    process->thread->frame = (TrapFrame *)(process->thread->kernel_stack_top - TF_SIZE);
    process->thread->context.ra = (uintptr_t)first_run;
    process->thread->context.sp = (uintptr_t)process->thread->frame; /* 16-byte aligned, below frame. */
    process->thread->frame->registers[2] = USER_STACK_TOP;
    process->thread->frame->registers[10] = (uint64_t)program;
    process->thread->frame->sepc = USER_ENTRY;
    uintptr_t status;
    __asm__ volatile("csrr %0, sstatus" : "=r"(status));
    process->thread->frame->sstatus = (status &
        ~((1UL << 8) | (1UL << 5) | (1UL << 1) | (1UL << 18) | (1UL << 19) |
          (3UL << 13) | (3UL << 9))) | (1UL << 5); /* SPIE: sret enables S interrupts. */
    if (publish) {
        publish_process(process, "initialized");
    }
    return process->pid;
}

void process_init(void) {
    if (vm_unmap_kernel_guard((uintptr_t)scheduler_stack) < 0) {
        console_puts("[process] Cannot install scheduler stack guard\n");
        kernel_idle();
    }
    thread_init();
    scheduler_init();
    workstate_init();
    if (create_process(0, 0, 1) != 1) {
        console_puts("[process] Cannot create init\n");
        kernel_idle();
    }
}

long process_start_gui_terminal(TtyOutputConsumer consumer, void *context, TtyId *id_out) {
    if (!consumer || !id_out) return ERR_INVALID;
    const TtyId id = tty_session_create(consumer, context);
    if (!id) return ERR_NO_SLOT;
    const long pid = create_process(0, 0, 0);
    if (pid < 0) {
        (void)tty_session_close(id);
        tty_release(id);
        return pid;
    }
    Process *process = find_pid(pid);
    if (!process || io_attach_tty(process, id) < 0) {
        if (process) {
            io_close_process(process);
            if (vm_destroy(process->page_table) == 0) {
                process->page_table = 0;
                discard_initial_thread(process);
            } else {
                process->parent_pid = 0;
                mark_zombie(process, "terminal cleanup deferred");
            }
        }
        (void)tty_session_close(id);
        tty_release(id);
        return ERR_NO_SLOT;
    }
    process->terminal_session = id;
    publish_process(process, "graphical terminal ready");
    *id_out = id;
    return pid;
}

long process_spawn(long program) {
    if (program != PROGRAM_WORKER && program != PROGRAM_QUICK) {
        return ERR_INVALID;
    }
    Process *parent = process_current();
    const long pid = create_process(program, parent->pid, 0);
    if (pid < 0) return pid;
    Process *child = find_pid(pid);
    io_inherit(child, parent);
    publish_process(child, "initialized with inherited descriptors");
    return pid;
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
        if (vm_destroy(child->page_table) == 0) {
            child->page_table = 0;
            discard_initial_thread(child);
        } else {
            child->parent_pid = 0;
            mark_zombie(child, "fork cleanup deferred");
        }
        return copy_error;
    }
    volatile unsigned char *to = (volatile unsigned char *)child->thread->frame;
    const unsigned char *from = (const unsigned char *)parent->thread->frame;
    for (size_t i = 0; i < sizeof(TrapFrame); ++i) {
        to[i] = from[i];
    }
    child->thread->frame->registers[10] = 0;
    io_inherit(child, parent);
    publish_process(child, "fork copied user state");
    return pid;
}

static long process_install_image(Process *process, uintptr_t root, long program, uintptr_t entry, uintptr_t stack);

long process_exec(long program, const char *argument) {
    if (program < PROGRAM_WORKER || (program > PROGRAM_FAN && program != PROGRAM_WORKSTATE) ||
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
        if (error < 0) { (void)vm_destroy(root); return error; }
    }
    return process_install_image(process, root, program, USER_ENTRY, stack);
}

long process_exec_path(const char *path) {
    uintptr_t root, entry;
    long result = elf_load(path, &root, &entry);
    if (result < 0) return result;
    return process_install_image(process_current(), root, PROGRAM_ELF, entry, USER_STACK_TOP);
}

static long process_install_image(Process *process, uintptr_t root, long program, uintptr_t entry, uintptr_t stack) {
    int error = thread_cancel_defer_enter();
    if (error < 0) { (void)vm_destroy(root); return error; }
    error = vm_scope_enter();
    if (error < 0) {
        (void)vm_destroy(root);
        (void)thread_cancel_defer_leave();
        return error;
    }
    sync_thread_cleanup(process->thread); /* New image must not inherit held mutexes. */
    (void)io_console_mode(0); /* Successful exec resets raw input ownership. */
    const uintptr_t old_root = process->page_table;
    const uint64_t status = process->thread->frame->sstatus;
    volatile unsigned char *bytes = (volatile unsigned char *)process->thread->frame;
    for (size_t i = 0; i < sizeof(TrapFrame); ++i) {
        bytes[i] = 0;
    }
    process->thread->frame->registers[2] = stack;
    process->thread->frame->registers[10] = program == PROGRAM_ELF ? 0 : (uint64_t)program;
    process->thread->frame->registers[11] = program == PROGRAM_FAN ? stack : 0;
    process->thread->frame->sepc = entry;
    process->thread->frame->sstatus = status;
    process->program = program;
    process->page_table = root;
    process->thread->wait_reason = WAIT_NONE;
    process->thread->channel = process->thread->wait_pid = 0;
    process->thread->wait_status = 0;
    vm_enable(root);
    (void)vm_destroy(old_root);
    console_puts("[exec] PID=");
    console_number(process->pid);
    console_puts(program == PROGRAM_ELF ? " retained; loaded WUFS ELF, program=" :
                 " retained; loaded app image, program=");
    console_number(program);
    console_putc('\n');
    error = vm_scope_leave();
    const int defer_error = thread_cancel_defer_leave();
    return error < 0 ? error : defer_error;
}

long thread_yield(void) {
    thread_transition(thread_current(), THREAD_READY, "yield");
    suspend_current();
    return 0;
}

void thread_preempt(void) {
    Thread *thread = thread_current();
    ++thread->preemptions;
    thread_transition(thread, THREAD_READY, "time slice expired");
    suspend_current();
}

long thread_sleep(long channel) {
    if (channel <= 0) {
        return ERR_INVALID;
    }
    Thread *thread = thread_current();
    thread->wait_reason = WAIT_CHANNEL;
    thread->channel = channel;
    thread_transition(thread, THREAD_WAITING, "sleep channel");
    suspend_current();
    return 0;
}

long thread_sleep_ms(long milliseconds) {
    if (milliseconds < 1 || milliseconds > 60000) return ERR_INVALID;
    const uint64_t deadline = clock_ticks() + clock_ms_ticks((unsigned long)milliseconds);
    return thread_wait_event(0, 0, 0, deadline, 0);
}

long thread_wakeup(long channel) {
    if (channel <= 0) {
        return ERR_INVALID;
    }
    long count = 0;
    for (int i = 0; i < THREAD_LIMIT; ++i) {
        Thread *thread = thread_slot(i);
        if (thread->state == THREAD_WAITING &&
            thread->wait_reason == WAIT_CHANNEL && thread->channel == channel) {
            thread->wait_reason = WAIT_NONE;
            thread->channel = 0;
            thread_transition(thread, THREAD_READY, "wakeup channel");
            ++count;
        }
    }
    return count;
}

static int reap(Process *process, int may_wait) {
    /* The caller must be on another root; kernel stacks remain static. */
    const int error = may_wait ? vm_destroy(process->page_table) : vm_try_destroy(process->page_table);
    if (error < 0) return error;
    process->page_table = 0;
    discard_initial_thread(process);
    return 0;
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
    const long exit_status = child->exit_status;
    const int reap_error = reap(child, 1);
    if (reap_error < 0) return reap_error;
    console_puts("[wait] collected PID=");
    console_number(pid);
    console_puts(" status=");
    console_number(exit_status);
    console_putc('\n');
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
    parent->thread->wait_reason = WAIT_CHILD;
    parent->thread->wait_pid = pid;
    parent->thread->wait_status = status_address;
    thread_transition(parent->thread, THREAD_WAITING, "wait child");
    suspend_current();
    /* Resume inside wait, on this parent's original kernel stack. */
    if (parent->thread->kill_pending) return ERR_INTERRUPTED;
    parent->thread->wait_reason = WAIT_NONE;
    parent->thread->wait_pid = 0;
    parent->thread->wait_status = 0;
    child = find_pid(pid);
    return child != 0 && child->parent_pid == parent->pid && child->state == PROCESS_ZOMBIE
        ? collect_child(parent, child, status_address) : ERR_NO_CHILD;
}

static void terminate(Process *process, long status) {
    const TtyId session = process->terminal_session;
    process->state = PROCESS_DYING;
    sync_thread_cleanup(process->thread);
    tty_release_raw_owner(process->thread->tid);
    io_cleanup_process_terminal(process, process->program == PROGRAM_FAN || process->program == PROGRAM_WORKSTATE,
                                session);
    process->terminal_session = TTY_ID_INVALID;
    process->thread->event_wait = (KernelEventWait){0};
    process->thread->wait_reason = WAIT_NONE;
    process->thread->channel = 0;
    process->thread->wait_pid = 0;
    process->thread->wait_status = 0;
    process->exit_status = status;
    mark_zombie(process, "exit");
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
    if (parent != 0 && parent->thread->state == THREAD_WAITING &&
        parent->thread->wait_reason == WAIT_CHILD && parent->thread->wait_pid == process->pid) {
        thread_transition(parent->thread, THREAD_READY, "child exited");
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
        target->parent_pid != process_current()->pid || target->state == PROCESS_NEW ||
        target->state == PROCESS_ZOMBIE) {
        return ERR_INVALID;
    }
    if (target->thread->kernel_call_active || target->thread->event_wait.active) {
        target->thread->kill_pending = 1;
        if (!target->thread->cancel_defer_depth) thread_cancel_wait(target->thread);
        return 0;
    }
    terminate(target, -9);
    return 0;
}

static void thread_cancel_wait(Thread *thread) {
    if (thread->event_wait.active) {
        thread_event_cancel(thread, ERR_INTERRUPTED);
        return;
    }
    switch (thread->wait_reason) {
    case WAIT_CHANNEL:
        thread->channel = 0;
        thread->wait_reason = WAIT_NONE;
        if (thread->state == THREAD_WAITING) thread_transition(thread, THREAD_READY, "wait cancelled");
        break;
    case WAIT_CHILD:
        thread->wait_pid = 0;
        thread->wait_status = 0;
        thread->wait_reason = WAIT_NONE;
        if (thread->state == THREAD_WAITING) thread_transition(thread, THREAD_READY, "wait cancelled");
        break;
    case WAIT_SYNC:
        sync_thread_cancel_wait(thread);
        break;
    default:
        break;
    }
}

long thread_block_sync(void) {
    Thread *thread = thread_current();
    thread->wait_reason = WAIT_SYNC;
    thread_transition(thread, THREAD_WAITING, "sync wait");
    suspend_current();
    return thread->kill_pending ? ERR_INTERRUPTED : 0;
}

void thread_wake_sync(Thread *thread) {
    if (thread->state == THREAD_WAITING && thread->wait_reason == WAIT_SYNC) {
        thread->wait_reason = WAIT_NONE;
        thread_transition(thread, THREAD_READY, "sync wakeup");
    }
}

void thread_kernel_call_enter(void) {
    Thread *thread = thread_current_or_null();
    if (thread) thread->kernel_call_active = 1;
}

void thread_kernel_call_return(void) {
    Thread *thread = thread_current_or_null();
    if (!thread) return;
    if (thread->cancel_defer_depth || thread->internal_gate_depth) {
        console_puts("[gate] Internal scope leaked across trap return\n");
        kernel_idle();
    }
    thread->kernel_call_active = 0;
    if (thread->kill_pending) {
        thread->kill_pending = 0;
        process_exit(-9);
    }
}

static void complete_event_wait(Thread *thread, long result) {
    KernelEventWait *wait = &thread->event_wait;
    if (!wait->active || wait->completed) return;
    wait->result = result;
    wait->completed = 1;
    if (thread->state == THREAD_WAITING && thread->wait_reason == WAIT_EVENT)
        thread_transition(thread, THREAD_READY, "event completed");
}

long thread_wait_event(KernelEventSource *source, uint32_t generation, uint64_t sequence,
                        uint64_t deadline, long timeout_result) {
    Thread *thread = thread_current_or_null();
    if (!thread || thread->state != THREAD_RUNNING || thread->event_wait.active ||
        !thread_wait_allowed()) return ERR_BUSY;
    if (thread->kill_pending && !thread->cancel_defer_depth) return ERR_INTERRUPTED;
    uintptr_t status;
    __asm__ volatile("csrr %0, sstatus" : "=r"(status));
    if (status & (1UL << 1)) return ERR_BUSY;
    if (source) {
        if (!source->active || source->generation != generation) return ERR_IO;
        if (source->sequence != sequence) return 0;
    } else if (!deadline) return ERR_INVALID;
    if (deadline && (int64_t)(clock_ticks() - deadline) >= 0) return timeout_result;

    thread->event_wait = (KernelEventWait){
        .source = source,
        .generation = generation,
        .sequence = sequence,
        .deadline = deadline,
        .timeout_result = timeout_result,
        .active = 1
    };
    thread->wait_reason = WAIT_EVENT;
    thread_transition(thread, THREAD_WAITING, deadline ? "event/deadline wait" : "event wait");
    suspend_current();

    /* The result lives in the TCB; source may already name a reused slot. */
    const long result = thread->event_wait.result;
    thread->event_wait = (KernelEventWait){0};
    if (thread->wait_reason == WAIT_EVENT) thread->wait_reason = WAIT_NONE;
    return result;
}

void thread_event_notify(KernelEventSource *source, uint32_t generation, long result) {
    if (!source) return;
    for (int i = 0; i < THREAD_LIMIT; ++i) {
        Thread *thread = thread_slot(i);
        KernelEventWait *wait = &thread->event_wait;
        if (thread->state == THREAD_UNUSED || !wait->active || wait->completed ||
            wait->source != source || wait->generation != generation) continue;
        complete_event_wait(thread, result);
    }
}

void thread_event_poll_timeouts(void) {
    const uint64_t now = clock_ticks();
    for (int i = 0; i < THREAD_LIMIT; ++i) {
        Thread *thread = thread_slot(i);
        KernelEventWait *wait = &thread->event_wait;
        if (!wait->active || wait->completed || !wait->deadline) continue;
        if ((int64_t)(now - wait->deadline) >= 0) complete_event_wait(thread, wait->timeout_result);
    }
}

void thread_event_cancel(Thread *thread, long result) {
    if (!thread || !thread->event_wait.active || thread->cancel_defer_depth) return;
    /* Cancellation wins until the saved caller resumes and unwinds. */
    thread->event_wait.result = result;
    thread->event_wait.completed = 1;
    if (thread->state == THREAD_WAITING && thread->wait_reason == WAIT_EVENT)
        thread_transition(thread, THREAD_READY, "event cancelled");
}

static const char *program_name(long program) {
    static const char *names[] = { "shell", "worker", "quick", "writer", "reader", "counter", "fan", "elf", "workstate" };
    return names[program];
}

const char *process_name(const Process *process) { return program_name(process->program); }

static const char *waiting_for(const Thread *thread) {
    if (thread->state != THREAD_WAITING) {
        return "-";
    }
    if (thread->wait_reason == WAIT_CHILD) {
        return "child";
    }
    if (thread->wait_reason == WAIT_CHANNEL) {
        return "channel";
    }
    if (thread->wait_reason == WAIT_SYNC) return "sync";
    if (thread->wait_reason == WAIT_EVENT)
        return thread->event_wait.source ? "event" : "timer";
    return "-";
}

long process_list(void) {
    console_puts("PID PPID NAME STATE WAIT TID\n");
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
        console_puts(thread_state_name(process->thread->state));
        console_putc(' ');
        console_puts(waiting_for(process->thread));
        console_putc(' ');
        console_number(process->thread->tid);
        console_putc('\n');
    }
    console_puts("Free PCB slots: ");
    console_number(PROCESS_LIMIT - count);
    console_putc('\n');
    unsigned free_threads = 0;
    for (int i = 0; i < THREAD_LIMIT; ++i)
        if (thread_slot(i)->state == THREAD_UNUSED) ++free_threads;
    console_puts("Free TCB slots: "); console_number(free_threads); console_putc('\n');
    scheduler_print_ready();
    page_print_stats();
    return count;
}

long process_inspect(long pid) {
    const int scope_error = vm_scope_enter();
    if (scope_error < 0) return scope_error;
    const Process *process = find_pid(pid);
    if (process == 0) {
        (void)vm_scope_leave();
        return ERR_INVALID;
    }
    console_puts("PCB PID=");
    console_number(pid);
    console_puts(" PPID=");
    console_number(process->parent_pid);
    console_puts(" name=");
    console_puts(program_name(process->program));
    console_puts(" state=");
    console_puts(thread_state_name(process->thread->state));
    console_puts(" process_state=");
    console_puts(process_state_name(process->state));
    console_puts(" TID=");
    console_number(process->thread->tid);
    console_puts(" ready_queued=");
    console_number(process->thread->ready_queued);
    console_puts("\ncwd=");
    console_puts(process->cwd);
    console_puts("\ndispatches=");
    console_number((long)process->thread->dispatches);
    console_puts(" preemptions=");
    console_number((long)process->thread->preemptions);
    console_puts("\npage_table=");
    console_hex(process->page_table);
    console_puts(" kernel_stack_top=");
    console_hex(process->thread->kernel_stack_top);
    console_puts("\nkernel_context_sp=");
    console_hex(process->thread->context.sp);
    console_puts(" kernel_context_ra=");
    console_hex(process->thread->context.ra);
    console_puts(" (saved at last switch; stale while Running)");
    console_puts(" user_stack_top=");
    console_hex(process->thread->user_stack_top);
    if (process->thread->frame) {
        console_puts("\nframe=ready sepc=");
        console_hex(process->thread->frame->sepc);
        console_puts(" saved_sp=");
        console_hex(process->thread->frame->registers[2]);
    } else {
        console_puts("\nframe=initializing sepc=(unavailable) saved_sp=(unavailable)");
    }
    console_puts("\nwait=");
    console_puts(waiting_for(process->thread));
    console_puts(" channel=");
    console_number(process->thread->channel);
    console_puts(" child=");
    console_number(process->thread->wait_pid);
    console_puts(" exit_status=");
    console_number(process->exit_status);
    console_puts("\nFDs=");
    for (int fd = 0; fd < PROCESS_FD_LIMIT; ++fd) {
        console_number(process->fd_table[fd]);
        console_putc(fd + 1 == PROCESS_FD_LIMIT ? '\n' : ' ');
    }
    console_puts("\nwait_source=");
    console_hex((uintptr_t)process->thread->event_wait.source);
    console_puts(" generation=");
    console_number(process->thread->event_wait.generation);
    console_puts(" sequence=");
    console_number((long)process->thread->event_wait.sequence);
    console_puts(" deadline=");
    console_number((long)process->thread->event_wait.deadline);
    console_puts(" completed=");
    console_number(process->thread->event_wait.completed);
    console_puts(" result=");
    console_number(process->thread->event_wait.result);
    console_puts(" kernel_call_active=");
    console_number(process->thread->kernel_call_active);
    console_puts(" kill_pending=");
    console_number(process->thread->kill_pending);
    console_putc('\n');
    vm_print_space(process->page_table);
    const int leave_error = vm_scope_leave();
    if (leave_error < 0) return leave_error;
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
    (void)result_address;
    return ERR_NO_SYSCALL; /* Retained number; general pipes use SYS_PIPE. */
}

TrapFrame *thread_resume(void) {
    vm_use_kernel();
    process_poll_sources();
    if (thread_current()->state != THREAD_RUNNING) suspend_current();
    vm_enable(thread_current()->process->page_table);
    return thread_current()->frame;
}

static _Noreturn void scheduler_loop(void) {
    for (;;) {
        vm_use_kernel();
        thread_set_current(0); /* Scheduler must not impersonate the last TCB. */
        thread_event_poll_timeouts();
        process_poll_sources();
        thread_event_poll_timeouts();
        for (int i = 0; i < PROCESS_LIMIT; ++i) {
            if (processes[i].state == PROCESS_ZOMBIE && processes[i].parent_pid == 0)
                (void)reap(&processes[i], 0);
        }
        Thread *next = scheduler_next();
        if (next) {
            thread_set_current(next);
            thread_transition(next, THREAD_RUNNING, "schedule");
            ++next->dispatches;
            vm_enable(next->process->page_table);
            timer_start_slice(SCHEDULER_QUANTUM_MS);
            workstate_dispatch(next);
            context_switch(&scheduler_context, &next->context);
            continue;
        }
        if (thread_event_waiting() || process_ui_event_active() || block_poll_pending()) {
            const uint64_t poll_deadline = clock_ticks() + clock_ms_ticks(10);
            const uint64_t wake_deadline = thread_event_deadline(poll_deadline);
            while ((int64_t)(clock_ticks() - wake_deadline) < 0 &&
                   scheduler_ready_count() == 0 &&
                   (thread_event_waiting() || process_ui_event_active() || block_poll_pending())) {
                timer_start_deadline(wake_deadline);
                __asm__ volatile("wfi" : : : "memory");
                timer_stop();
                process_poll_sources();
                thread_event_poll_timeouts();
            }
            continue;
        }
        int waiting = 0;
        for (int i = 0; i < THREAD_LIMIT; ++i) {
            Thread *thread = thread_slot(i);
            if (thread->state != THREAD_WAITING) continue;
            ++waiting;
            console_puts("[scheduler] Waiting TID="); console_number(thread->tid);
            console_puts(" PID="); console_number(thread->process->pid);
            console_puts(" for "); console_puts(waiting_for(thread)); console_putc('\n');
        }
        console_puts(waiting ? "[scheduler] No Ready thread; blocked without an event source\n"
                            : "[scheduler] All processes reaped; idle\n");
        kernel_idle();
    }
}

_Noreturn void process_start(void) {
    runtime_started = 1;
    scheduler_context.ra = (uintptr_t)scheduler_loop;
    scheduler_context.sp = (uintptr_t)(scheduler_stack + PAGE_SIZE + PROCESS_KERNEL_STACK_SIZE);
    console_puts("[context] RV64 swtch ready; dedicated scheduler stack\n");
    context_switch(&boot_context, &scheduler_context);
    kernel_idle();
}
