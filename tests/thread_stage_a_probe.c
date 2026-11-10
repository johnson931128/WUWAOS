/* Runs inside WUWAOS through its existing ELF/syscall ABI. No thread ABI yet. */
#include "user/lib/api.h"
#include "user/lib/print.h"
#include "syscall_numbers.h"
#include "sync_ops.h"
#include <stdint.h>

#define CHECK(test) do { if (!(test)) { \
    user_puts("FAIL: stage A line "); user_number(__LINE__); user_puts("\n"); exit(90); \
} } while (0)

static long private_data = 10;
static WorkstateSnapshot snapshot;

static void collected(long pid, long expected) {
    long status = 12345;
    CHECK(pid > 0 && wait(pid, &status) == pid && status == expected);
}

static void busy(void) {
    uint64_t start, now;
    __asm__ volatile("rdtime %0" : "=r"(start));
    do { __asm__ volatile("rdtime %0" : "=r"(now)); }
    while (now - start < 1800000UL); /* 180 ms at QEMU virt's 10 MHz timebase. */
}

long user_main(long unused) {
    (void)unused;
    CHECK(trace(0) == 0);
    for (int i = 0; i < 20; ++i) {
        volatile long stack_value = 20;
        long pid = fork();
        CHECK(pid >= 0);
        if (!pid) { private_data = 99; stack_value = 88; exit(7); }
        collected(pid, 7);
        CHECK(private_data == 10 && stack_value == 20);
    }
    user_puts("PASS: fork isolation and repeated PCB/TCB reuse\n");

    long pid = fork(); CHECK(pid >= 0);
    if (!pid) { (void)exec(PROGRAM_QUICK); exit(99); }
    collected(pid, 40);
    pid = spawn(PROGRAM_WORKER); CHECK(pid > 0);
    CHECK(sleep_ms(20) == 0);
    CHECK(wakeup(7) == 1); collected(pid, 0);
    user_puts("PASS: exec, spawn, channel wake and child wait\n");

    long ends[2]; CHECK(pipe(ends) == 0);
    pid = fork(); CHECK(pid >= 0);
    if (!pid) {
        CHECK(close(ends[0]) == 0);
        static const char bytes[] = "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        CHECK(user_write_all(ends[1], bytes, sizeof(bytes) - 1U) == 0);
        CHECK(close(ends[1]) == 0); exit(0);
    }
    CHECK(close(ends[1]) == 0);
    char buffer[13]; unsigned long total = 0;
    for (;;) {
        long count = read(ends[0], buffer, sizeof(buffer)); CHECK(count >= 0);
        if (!count) break;
        total += (unsigned long)count;
    }
    CHECK(total == 62 && close(ends[0]) == 0); collected(pid, 0);
    CHECK(pipe(ends) == 0);
    pid = fork(); CHECK(pid >= 0);
    if (!pid) { CHECK(close(ends[1]) == 0); (void)read(ends[0], buffer, 1); exit(99); }
    CHECK(sleep_ms(20) == 0); CHECK(kill(pid) == 0); collected(pid, -9);
    CHECK(close(ends[0]) == 0 && close(ends[1]) == 0);
    user_puts("PASS: blocking pipe, EOF and cancellation of event waiter\n");

    long sem = semaphore_create(0); CHECK(sem > 0);
    pid = fork(); CHECK(pid >= 0);
    if (!pid) { (void)semaphore_wait(sem); exit(99); }
    CHECK(sleep_ms(20) == 0); CHECK(kill(pid) == 0); collected(pid, -9);
    CHECK(sync_destroy(sem) == 0);
    long mutex = mutex_create(); sem = semaphore_create(0); CHECK(mutex > 0 && sem > 0);
    pid = fork(); CHECK(pid >= 0);
    if (!pid) {
        CHECK(mutex_lock(mutex) == 0 && semaphore_post(sem) == 0);
        (void)sleep(31); exit(99);
    }
    CHECK(semaphore_wait(sem) == 0); CHECK(kill(pid) == 0); collected(pid, -9);
    CHECK(mutex_lock(mutex) == SYNC_ERR_OWNER_DIED);
    CHECK(sync_destroy(mutex) == 0 && sync_destroy(sem) == 0);
    user_puts("PASS: sync waiter cancellation and mutex owner death\n");

    pid = fork(); CHECK(pid >= 0);
    if (!pid) { busy(); exit(0); }
    busy(); CHECK(sleep_ms(250) == 0);
    CHECK(pcb(getpid()) == 0 && pcb(pid) == 0); /* Host checks nonzero preemptions. */
    collected(pid, 0);
    CHECK(workstate_snapshot(&snapshot) == 0 && snapshot.version == WORKSTATE_VERSION);
    CHECK(snapshot.count == 2); /* UART shell + this probe; all children reaped. */
    user_puts("PASS: user timer, independent kernel stacks and workstate snapshot\n");

    CHECK(vmstat(8) == 0);
    long memory = sbrk(16UL * 4096UL); CHECK(memory > 0);
    volatile unsigned char *pages = (volatile unsigned char *)(uintptr_t)memory;
    for (int i = 0; i < 16; ++i) pages[i * 4096] = (unsigned char)(i + 17);
    for (int i = 0; i < 16; ++i) CHECK(pages[i * 4096] == (unsigned char)(i + 17));
    CHECK(sbrk(-16L * 4096L) > 0 && vmstat(64) == 0);
    pid = fork(); CHECK(pid >= 0);
    if (!pid) { *(volatile long *)(uintptr_t)0x20000000UL = 1; exit(99); }
    collected(pid, ERR_ADDRESS);
    user_puts("PASS: VM swap, user fault and address-space cleanup\n");

    long children[PROCESS_LIMIT - 2];
    for (int i = 0; i < PROCESS_LIMIT - 2; ++i) {
        pid = fork(); CHECK(pid >= 0);
        if (!pid) { (void)sleep(55); exit(0); }
        children[i] = pid;
    }
    CHECK(fork() == ERR_NO_SLOT);
    for (int i = 0; i < PROCESS_LIMIT - 2; ++i) {
        CHECK(kill(children[i]) == 0); collected(children[i], -9);
    }
    CHECK(ps() == 2);
    user_puts("PASS: PCB capacity and complete TCB reclamation\n");
    user_puts("PASS: THREAD STAGE A\n");
    return 0;
}
