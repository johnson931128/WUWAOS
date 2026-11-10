#include "kernel/proc/workstate.h"
#include "workstate_info.h"
#include "arch/riscv/clock.h"
#include "kernel/mm/vm.h"
#include "kernel/mm/swap.h"
#include "syscall_numbers.h"

static uint64_t busy_ticks;

void workstate_init(void) { busy_ticks = 0; }
void workstate_dispatch(Thread *thread) { thread->run_started = clock_ticks(); }
void workstate_pause(Thread *thread) {
    const uint64_t elapsed = clock_ticks() - thread->run_started;
    thread->cpu_ticks += elapsed;
    busy_ticks += elapsed; /* Keep exited/reaped processes in the system total. */
}

static void copy_label(char *to, const char *from, size_t capacity) {
    size_t i = 0;
    while (i + 1 < capacity && from[i]) { to[i] = from[i]; ++i; }
    to[i] = 0;
}

long workstate_snapshot(uintptr_t address, size_t length) {
    if (length != sizeof(WorkstateSnapshot)) return ERR_INVALID;
    Process *caller = process_current();
    if (!user_writable(caller->page_table, address, length)) return ERR_ADDRESS;
    int scope_error = vm_scope_enter();
    if (scope_error < 0) return scope_error;
    WorkstateSnapshot snapshot = {0};
    snapshot.version = WORKSTATE_VERSION;
    snapshot.ticks = clock_ticks();
    snapshot.timebase_hz = CLOCK_TIMEBASE_HZ;
    snapshot.busy_ticks = busy_ticks;
    snapshot.ram_bytes = RAM_END - RAM_BASE;
    snapshot.page_bytes = PAGE_SIZE;
    size_t free_pages, pool_pages;
    page_get_stats(&free_pages, &pool_pages);
    snapshot.free_bytes = free_pages * PAGE_SIZE;
    snapshot.reserved_bytes = snapshot.ram_bytes - pool_pages * PAGE_SIZE;
    snapshot.swap_used_pages = swap_used();
    snapshot.swap_total_pages = swap_total();
    for (int i = 0; i < PROCESS_LIMIT; ++i) {
        const Process *process = process_slot(i);
        if (process->state == PROCESS_UNUSED) continue;
        WorkstateProcess *row = &snapshot.processes[snapshot.count++];
        row->pid = process->pid;
        row->parent_pid = process->parent_pid;
        copy_label(row->name, process_name(process), sizeof(row->name));
        copy_label(row->state, process->thread ? thread_state_name(process->thread->state) :
                   process_state_name(process->state), sizeof(row->state));
        row->cpu_ticks = process->retired_cpu_ticks;
        for (int t = 0; t < THREAD_LIMIT; ++t) {
            const Thread *thread = thread_slot(t);
            if (thread->state == THREAD_UNUSED || thread->process != process) continue;
            row->cpu_ticks += thread->cpu_ticks;
            if (thread == thread_current_or_null() && process == caller) {
                const uint64_t pending = snapshot.ticks - thread->run_started;
                row->cpu_ticks += pending;
                snapshot.busy_ticks += pending;
            }
        }
        VmUsage usage;
        const int usage_error = vm_get_usage(process->page_table, &usage);
        if (usage_error < 0) { (void)vm_scope_leave(); return usage_error; }
        row->resident_pages = usage.resident_pages;
        row->swapped_pages = usage.swapped_pages;
        row->heap_bytes = usage.heap_bytes;
    }
    const int leave_error = vm_scope_leave();
    if (leave_error < 0) return leave_error;
    return user_copy_to(caller->page_table, address, &snapshot, sizeof(snapshot));
}
