#include "workstate.h"
#include "api.h"
#include "print.h"
#include <stddef.h>

static void field(const char *text, unsigned int width) {
    unsigned int length = 0;
    while (text[length]) ++length;
    user_puts(text);
    while (length++ < width) user_puts(" ");
}

static void number(uint64_t value, unsigned int width) {
    char text[21];
    unsigned int length = 0;
    do { text[length++] = (char)('0' + value % 10); value /= 10; } while (value);
    for (unsigned int i = 0; i < length / 2; ++i) {
        const char saved = text[i]; text[i] = text[length - i - 1]; text[length - i - 1] = saved;
    }
    text[length] = 0;
    field(text, width);
}

/* Bounded long division: floor(part * 1000 / total), without 64-bit overflow. */
static uint64_t ratio(uint64_t part, uint64_t total) {
    if (total == 0) return 0;
    if (part >= total) return 1000;
    uint64_t scaled = (part / total) * 1000, remainder = part % total;
    for (int i = 0; i < 3; ++i) {
        uint64_t digit = 0, next = 0;
        for (int j = 0; j < 10; ++j) {
            if (next >= total - remainder) { next -= total - remainder; ++digit; }
            else next += remainder;
        }
        scaled = scaled * 10 + digit;
        remainder = next;
    }
    return scaled;
}

static void percent(uint64_t value) {
    number(value / 10, 0); user_puts("."); number(value % 10, 0); user_puts("%");
    if (value < 100) user_puts(" ");
    if (value < 1000) user_puts(" ");
    user_puts(" ");
}

static uint64_t previous_cpu(const WorkstateSnapshot *previous, int64_t pid) {
    for (uint64_t i = 0; i < previous->count; ++i) {
        if (previous->processes[i].pid == pid) return previous->processes[i].cpu_ticks;
    }
    return 0; /* PID created during this interval; never compare reused PCB slots. */
}

static void draw(const WorkstateSnapshot *now, const WorkstateSnapshot *previous) {
    const uint64_t elapsed = now->ticks - previous->ticks;
    const uint64_t cpu = ratio(now->busy_ticks - previous->busy_ticks, elapsed);
    user_puts("\033[H\033[1;36mworkstate\033[0m | refresh 1s | q: return to shell\033[K\n");
    user_puts("CPU task: ");
    if (elapsed) percent(cpu); else user_puts("sampling... ");
    user_puts(" idle/scheduler: ");
    if (elapsed) percent(1000 - cpu); else user_puts("sampling...");
    user_puts("\033[K\nRAM: ");
    number((now->ram_bytes - now->free_bytes) / 1024, 0); user_puts("/");
    number(now->ram_bytes / 1024, 0); user_puts(" KiB used ");
    percent(ratio(now->ram_bytes - now->free_bytes, now->ram_bytes));
    user_puts(" free "); number(now->free_bytes / 1024, 0); user_puts(" KiB\033[K\n");
    user_puts("Reserved/static: "); number(now->reserved_bytes / 1024, 0);
    user_puts(" KiB | Swap slots: "); number(now->swap_used_pages, 0); user_puts("/");
    number(now->swap_total_pages, 0); user_puts("\033[K\nProcesses: "); number(now->count, 0);
    user_puts(" | monitor runs inside shell\033[K\n\033[K\n");
    user_puts("PID  PPID NAME     STATE    CPU%    RSS KiB MEM%    SWAP KiB CPU ms\033[K\n");
    for (uint64_t i = 0; i < now->count; ++i) {
        const WorkstateProcess *row = &now->processes[i];
        number((uint64_t)row->pid, 5); number((uint64_t)row->parent_pid, 5);
        field(row->name, 9); field(row->state, 9);
        if (elapsed) percent(ratio(row->cpu_ticks - previous_cpu(previous, row->pid), elapsed));
        else field("--", 7);
        number(row->resident_pages * now->page_bytes / 1024, 8);
        percent(ratio(row->resident_pages * now->page_bytes, now->ram_bytes));
        number(row->swapped_pages * now->page_bytes / 1024, 9);
        number(row->cpu_ticks / (now->timebase_hz / 1000), 0);
        user_puts("\033[K\n");
    }
    user_puts("\033[J\nCPU counts dispatched user + kernel time, not host CPU load.\n");
    user_puts("Idle includes scheduler/UART polling; RSS excludes kernel stacks/tables.\n");
    user_puts("RAM used includes reserved/static memory; MEM% uses total RAM.\n");
}

void workstate_main(void) {
    long result = console_mode(1);
    if (result < 0) { user_puts("workstate: console is busy\n"); return; }
    WorkstateSnapshot previous, current;
    result = workstate_snapshot(&previous);
    if (result == 0 && previous.version != WORKSTATE_VERSION) result = -1;
    if (result == 0) {
        user_puts("\033[?1049h\033[2J\033[?25l");
        draw(&previous, &previous);
        int done = 0;
        while (!done) {
            for (int i = 0; i < 32; ++i) {
                const long key = console_poll();
                if (key < 0) { result = key; done = 1; break; }
                if (key == 0) break;
                if (key - 1 == 'q' || key - 1 == 'Q') { done = 1; break; }
            }
            if (done) break;
            result = workstate_snapshot(&current);
            if (result < 0) break;
            if (current.ticks - previous.ticks >= current.timebase_hz) {
                draw(&current, &previous);
                volatile unsigned char *to = (volatile unsigned char *)&previous;
                const unsigned char *from = (const unsigned char *)&current;
                for (size_t i = 0; i < sizeof(previous); ++i) to[i] = from[i];
            }
            result = sleep_ms(50);
            if (result < 0) break;
        }
        user_puts("\033[0m\033[?25h\033[?1049l");
    }
    (void)console_mode(0);
    if (result < 0) { user_puts("workstate error: "); user_number(result); user_puts("\n"); }
}
