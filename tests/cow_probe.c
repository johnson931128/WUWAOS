#include "syscall_numbers.h"
#include "user/lib/api.h"
#include "user/lib/print.h"
#include "vm_limits.h"

#define CHECK(test) do { if (!(test)) { user_puts("FAIL: COW line "); \
    user_number(__LINE__); user_puts("\n"); return 1; } } while (0)

static volatile unsigned char *arena;
static WorkstateSnapshot snapshot;

static volatile unsigned char *cell(unsigned int page) {
    return arena + page * VM_PAGE_BYTES;
}

static int allocate(unsigned int count) {
    long address = sbrk(count * VM_PAGE_BYTES);
    if (address < 0) return 0;
    arena = (volatile unsigned char *)(uintptr_t)address;
    for (unsigned int i = 0; i < count; ++i) *cell(i) = (unsigned char)(i + 10);
    return 1;
}

static int collect(long pid, long expected) {
    long status = 1;
    return wait(pid, &status) == pid && status == expected;
}

static unsigned long ticks(void) {
    unsigned long now;
    __asm__ volatile("rdtime %0" : "=r"(now) : : "memory");
    return now;
}

static long isolation(void) {
    CHECK(allocate(6));
    const long lazy = sbrk(VM_PAGE_BYTES);
    CHECK(lazy == (long)(uintptr_t)cell(6));
    long request[2], reply[2];
    CHECK(pipe(request) == 0 && pipe(reply) == 0);
    long child = fork();
    CHECK(child >= 0);
    if (child == 0) {
        CHECK(*cell(0) == 10 && *cell(1) == 11);
        CHECK(write(reply[1], "r", 1) == 1);
        CHECK(read(request[0], (char *)cell(2), 1) == 1 && *cell(2) == 'x');
        CHECK(*cell(0) == 10 && *cell(6) == 0);
        for (unsigned int i = 0; i < 64; ++i) *cell(1) = (unsigned char)i;
        CHECK(write(reply[1], "w", 1) == 1);
        char token;
        CHECK(read(request[0], &token, 1) == 1);
        (void)exec_path("/bin/cow.elf");
        return 1;
    }
    char token;
    CHECK(read(reply[0], &token, 1) == 1 && token == 'r');
    user_puts("COW SHARING\n");
    CHECK(vmstat(0) == 0);
    *cell(0) = 41;
    CHECK(write(request[1], "x", 1) == 1);
    CHECK(read(reply[0], &token, 1) == 1 && token == 'w');
    CHECK(*cell(0) == 41 && *cell(1) == 11 && *cell(2) == 12);
    long sibling = fork();
    CHECK(sibling >= 0);
    if (sibling == 0) {
        CHECK(*cell(3) == 13);
        *cell(3) = 92;
        return 0;
    }
    CHECK(collect(sibling, 0) && *cell(3) == 13);
    CHECK(write(request[1], "e", 1) == 1 && collect(child, 0));
    user_puts("COW BEFORE REUSE\n");
    CHECK(vmstat(0) == 0);
    for (unsigned int i = 0; i < 64; ++i) *cell(4) = (unsigned char)i;
    CHECK(*cell(4) == 63);
    user_puts("COW AFTER REUSE\n");
    CHECK(vmstat(0) == 0);

    child = fork();
    CHECK(child >= 0);
    if (child == 0) {
        volatile unsigned char *code = (volatile unsigned char *)(uintptr_t)ticks;
        *code = 0;
        return 1;
    }
    CHECK(collect(child, ERR_ADDRESS));
    user_puts("PASS: COW isolation, copyout, refcount, lazy page, exec and RX protection\n");
    return 0;
}

static long pressure(int read_only) {
    CHECK(allocate(20));
    long request[2], reply[2];
    CHECK(pipe(request) == 0 && pipe(reply) == 0);
    long child = fork();
    CHECK(child >= 0);
    if (child == 0) {
        char token;
        CHECK(write(reply[1], "r", 1) == 1 && read(request[0], &token, 1) == 1);
        for (unsigned int i = 0; i < 20; ++i) CHECK(*cell(i) == i + 10);
        *cell(19) = 88;
        return 0;
    }
    char token;
    CHECK(read(reply[0], &token, 1) == 1);
    CHECK(vmstat(8) == (read_only ? ERR_IO : 0));
    user_puts("COW PRESSURE\n");
    CHECK(vmstat(0) == 0);
    CHECK(write(request[1], "x", 1) == 1 && collect(child, 0));
    for (unsigned int i = 0; i < 20; ++i) CHECK(*cell(i) == i + 10);
    CHECK(vmstat(VM_DEFAULT_FRAMES) == 0);
    user_puts("PASS: COW shared pressure\n");
    return 0;
}

static long rollback(void) {
    CHECK(vmstat(8) == 0 && allocate(16) && vmstat(VM_DEFAULT_FRAMES) == 0);
    user_puts("COW BEFORE FORK FAILURE\n");
    CHECK(vmstat(0) == 0);
    long child = fork();
    if (child == 0) return 1;
    CHECK(child == ERR_NO_MEMORY);
    user_puts("COW AFTER FORK FAILURE\n");
    CHECK(vmstat(0) == 0);
    for (unsigned int i = 0; i < 16; ++i) CHECK(*cell(i) == i + 10);
    *cell(15) = 99;
    CHECK(*cell(15) == 99);
    user_puts("PASS: COW fork rollback\n");
    return 0;
}

static long no_frame(void) {
    CHECK(allocate(1));
    long request[2], reply[2];
    CHECK(pipe(request) == 0 && pipe(reply) == 0);
    long child = fork();
    CHECK(child >= 0);
    char token;
    if (child == 0) {
        CHECK(write(reply[1], "r", 1) == 1 && read(request[0], &token, 1) == 1);
        CHECK(*cell(0) == 10);
        return 0;
    }
    CHECK(read(reply[0], &token, 1) == 1);
    CHECK(vmstat(24) == 0 && sbrk(32 * VM_PAGE_BYTES) > 0);
    long result = 0;
    for (unsigned int i = 1; i <= 32; ++i) {
        result = getcwd((char *)cell(i), 128);
        if (result < 0) break;
    }
    CHECK(result == ERR_IO);
    user_puts("COW BEFORE NO FRAME\n");
    CHECK(vmstat(0) == 0);
    CHECK(getcwd((char *)cell(0), 128) == ERR_IO && *cell(0) == 10);
    user_puts("COW AFTER NO FRAME\n");
    CHECK(vmstat(0) == 0);
    CHECK(sbrk(-32L * VM_PAGE_BYTES) > 0);
    CHECK(write(request[1], "x", 1) == 1 && collect(child, 0));
    CHECK(getcwd((char *)cell(0), 128) >= 0 && *cell(0) == '/');
    user_puts("PASS: COW allocation failure preserves both mappings\n");
    return 0;
}

static long simultaneous(void) {
    CHECK(allocate(16));
    const unsigned long deadline = ticks() + 2000000UL;
    long child = fork();
    CHECK(child >= 0 && ticks() < deadline);
    while (ticks() < deadline) {}
    const unsigned char value = child == 0 ? 71 : 82;
    for (unsigned int i = 0; i < 16; ++i) *cell(i) = value;
    for (unsigned int i = 0; i < 16; ++i) CHECK(*cell(i) == value);
    if (child == 0) return 0;
    CHECK(collect(child, 0));
    for (unsigned int i = 0; i < 16; ++i) CHECK(*cell(i) == value);
    user_puts("PASS: COW paired stores\n");
    return 0;
}

static long benchmark(unsigned int count, char workload) {
    CHECK(count && count <= 48 && allocate(count));
    CHECK(workstate_snapshot(&snapshot) == 0);
    const long self = getpid();
    unsigned long resident = 0;
    for (unsigned long i = 0; i < snapshot.count; ++i)
        if (snapshot.processes[i].pid == self) resident = snapshot.processes[i].resident_pages;
    CHECK(resident >= count);
    user_puts("COW BENCH BEFORE\n");
    CHECK(vmstat(0) == 0);
    const unsigned long start = ticks();
    long child = fork();
    if (child == 0) {
        if (workload == 'e') {
            (void)exec_path("/bin/cow.elf");
            return 1;
        }
        for (unsigned int i = 0; i < count; ++i) {
            CHECK(*cell(i) == i + 10);
            if (workload == 'a' || (workload == 's' && i == 0)) *cell(i) = 99;
        }
        return 0;
    }
    const unsigned long fork_ticks = ticks() - start;
    CHECK(child > 0 && collect(child, 0));
    const unsigned long total_ticks = ticks() - start;
    user_puts("COW BENCH AFTER\n");
    CHECK(vmstat(0) == 0);
    user_puts("COW BENCH RESULT "); user_number((long)resident); user_puts(" ");
    user_number((long)fork_ticks); user_puts(" "); user_number((long)total_ticks); user_puts("\n");
    return 0;
}

long user_main(long argc, const char *const argv[]) {
    if (argc < 2) return 0;
    switch (argv[1][0]) {
    case 'i': return isolation();
    case 'p': return pressure(0);
    case 'e': return pressure(1);
    case 'n': return no_frame();
    case 'r': return rollback();
    case 's': return simultaneous();
    case 'b': {
        CHECK(argc == 4);
        unsigned int count = 0;
        for (const char *p = argv[2]; *p; ++p) count = count * 10 + (unsigned int)(*p - '0');
        return benchmark(count, argv[3][0]);
    }
    default: return 1;
    }
}
