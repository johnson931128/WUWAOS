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

static __attribute__((noinline)) void number(uint64_t value, unsigned int width) {
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

enum { COLUMNS = 9, CELL_SIZE = 24, LINE_SIZE = 256 };
static const char *const headers[COLUMNS] = {
    "PID", "PPID", "NAME", "STATE", "CPU%", "RSS(K)", "MEM%", "SWAP(K)", "CPUms"
};
static const unsigned int minimum_widths[COLUMNS] = {3, 4, 7, 7, 6, 6, 6, 7, 5};

static unsigned int text_length(const char *text) {
    unsigned int length = 0;
    while (text[length]) ++length;
    return length;
}

static __attribute__((noinline)) void decimal(char out[CELL_SIZE], uint64_t value) {
    unsigned int length = 0;
    do { out[length++] = (char)('0' + value % 10); value /= 10; } while (value);
    for (unsigned int i = 0; i < length / 2; ++i) {
        const char saved = out[i]; out[i] = out[length - i - 1]; out[length - i - 1] = saved;
    }
    out[length] = 0;
}

static void percentage(char out[CELL_SIZE], uint64_t value) {
    decimal(out, value / 10);
    unsigned int length = text_length(out);
    out[length++] = '.';
    out[length++] = (char)('0' + value % 10);
    out[length++] = '%';
    out[length] = 0;
}

static void cells(char out[COLUMNS][CELL_SIZE], const WorkstateProcess *row,
                  const WorkstateSnapshot *now, const WorkstateSnapshot *previous) {
    const uint64_t elapsed = now->ticks - previous->ticks;
    decimal(out[0], (uint64_t)row->pid);
    decimal(out[1], (uint64_t)row->parent_pid);
    for (unsigned int column = 2; column <= 3; ++column) {
        const char *source = column == 2 ? row->name : row->state;
        unsigned int i = 0;
        while (i + 1 < CELL_SIZE && source[i]) { out[column][i] = source[i]; ++i; }
        out[column][i] = 0;
    }
    if (elapsed) percentage(out[4], ratio(row->cpu_ticks - previous_cpu(previous, row->pid), elapsed));
    else { out[4][0] = '-'; out[4][1] = '-'; out[4][2] = 0; }
    decimal(out[5], row->resident_pages * now->page_bytes / 1024);
    percentage(out[6], ratio(row->resident_pages * now->page_bytes, now->ram_bytes));
    decimal(out[7], row->swapped_pages * now->page_bytes / 1024);
    decimal(out[8], row->cpu_ticks / (now->timebase_hz / 1000));
}

static void border(const unsigned int widths[COLUMNS]) {
    char line[LINE_SIZE];
    unsigned int at = 0;
    line[at++] = '+';
    for (unsigned int column = 0; column < COLUMNS; ++column) {
        for (unsigned int i = 0; i < widths[column] + (column < 2 ? 1U : 2U); ++i) line[at++] = '-';
        line[at++] = '+';
    }
    line[at++] = '\033'; line[at++] = '['; line[at++] = 'K'; line[at++] = '\n';
    (void)user_write_all(1, line, at);
}

static void table_row(const char *const values[COLUMNS], const unsigned int widths[COLUMNS], int header) {
    char line[LINE_SIZE];
    unsigned int at = 0;
    line[at++] = '|';
    for (unsigned int column = 0; column < COLUMNS; ++column) {
        const unsigned int length = text_length(values[column]);
        const int right = !header && column != 2 && column != 3;
        if (column >= 2) line[at++] = ' ';
        if (right) for (unsigned int i = length; i < widths[column]; ++i) line[at++] = ' ';
        for (unsigned int i = 0; i < length; ++i) line[at++] = values[column][i];
        if (!right) for (unsigned int i = length; i < widths[column]; ++i) line[at++] = ' ';
        line[at++] = ' ';
        line[at++] = '|';
    }
    line[at++] = '\033'; line[at++] = '['; line[at++] = 'K'; line[at++] = '\n';
    (void)user_write_all(1, line, at);
}

static void process_table(const WorkstateSnapshot *now, const WorkstateSnapshot *previous) {
    unsigned int widths[COLUMNS];
    char rendered[COLUMNS][CELL_SIZE];
    const char *values[COLUMNS];
    for (unsigned int column = 0; column < COLUMNS; ++column) widths[column] = minimum_widths[column];
    for (uint64_t i = 0; i < now->count; ++i) {
        cells(rendered, &now->processes[i], now, previous);
        for (unsigned int column = 0; column < COLUMNS; ++column) {
            const unsigned int length = text_length(rendered[column]);
            if (length > widths[column]) widths[column] = length;
        }
    }
    border(widths);
    table_row(headers, widths, 1);
    border(widths);
    for (uint64_t i = 0; i < now->count; ++i) {
        cells(rendered, &now->processes[i], now, previous);
        for (unsigned int column = 0; column < COLUMNS; ++column) values[column] = rendered[column];
        table_row(values, widths, 0);
    }
    border(widths);
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
    process_table(now, previous);
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
