/* Deterministic tests of the real mutex code, with CPU/scheduler stand-ins.
 * No RV64 kernel, disk or QEMU is built/run by this test. Context-switch assembly
 * and real interrupt delivery still require the separate guest checks.
 */
#include "kernel/proc/kmutex.h"
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); exit(1); \
} } while (0)

static Thread threads[6];
static Thread *current;
static KernelMutex fs, vm;
static jmp_buf panic_jump, wait_jump;
static int expect_panic, queue_only, in_irq, wakes;
static void (*on_block)(void);
uintptr_t kmutex_test_sstatus;

Thread *thread_current_or_null(void) { return current; }
int irq_in_handler(void) { return in_irq; }
void console_puts(const char *text) { (void)text; }
void console_putc(char character) { (void)character; }
void console_number(long number) { (void)number; }
_Noreturn void kernel_idle(void) {
    CHECK(expect_panic);
    longjmp(panic_jump, 1);
}
void thread_wake_sync(Thread *process) {
    CHECK(process->state == THREAD_WAITING && process->wait_reason == WAIT_SYNC);
    /* Prove ownership is reserved before the waiter becomes Ready. */
    CHECK(process->kernel_waiting == 0 && process->kernel_locks != 0);
    process->state = THREAD_READY;
    process->wait_reason = WAIT_NONE;
    ++wakes;
}
long thread_block_sync(void) {
    current->state = THREAD_WAITING;
    current->wait_reason = WAIT_SYNC;
    if (queue_only) longjmp(wait_jump, 1);
    CHECK(on_block != 0);
    on_block();
    return 0;
}

#define PANICS(statement) do { \
    expect_panic = 1; \
    if (setjmp(panic_jump) == 0) { statement; CHECK(0); } \
    expect_panic = 0; \
} while (0)

static void as_thread(int index) {
    current = &threads[index];
    current->state = THREAD_RUNNING;
}
static void reset(void) {
    memset(threads, 0, sizeof(threads));
    for (int i = 0; i < 6; ++i) threads[i].tid = i + 1;
    fs = (KernelMutex)KMUTEX_INITIALIZER("FS", KMUTEX_RANK_FS);
    vm = (KernelMutex)KMUTEX_INITIALIZER("VM", KMUTEX_RANK_VM);
    current = 0;
    kmutex_test_sstatus = 0;
    in_irq = wakes = queue_only = 0;
    on_block = 0;
}
static void queue_thread(int index) {
    as_thread(index);
    queue_only = 1;
    /* Preserve the real enqueue result while standing in for a suspended stack. */
    if (setjmp(wait_jump) == 0) { kmutex_lock(&fs); CHECK(0); }
    queue_only = 0;
    CHECK(current->kernel_waiting == &fs && current->state == THREAD_WAITING);
}

static void test_context_and_order(void) {
    reset();
    kmutex_lock(&fs); /* No-process owner must differ from an unlocked mutex. */
    CHECK(fs.locked && !fs.owner);
    PANICS(kmutex_lock(&fs));
    kmutex_lock(&vm);
    PANICS(kmutex_unlock(&fs));
    kmutex_unlock(&vm);
    kmutex_unlock(&fs);
    as_thread(0);
    kmutex_lock(&vm);
    PANICS(kmutex_lock(&fs));
    kmutex_unlock(&vm);
    kmutex_test_sstatus = 2;
    PANICS(kmutex_lock(&fs));
    kmutex_test_sstatus = 0;
    in_irq = 1;
    PANICS(kmutex_trylock(&fs));
    in_irq = 0;
    current->state = THREAD_WAITING;
    PANICS(kmutex_lock(&fs));
    as_thread(0);
    kmutex_lock(&fs);
    as_thread(1);
    PANICS(kmutex_unlock(&fs));
    CHECK(!kmutex_trylock(&fs));
    current = 0;
    PANICS(kmutex_lock(&fs));
    CHECK(fs.waiters == 0);
    as_thread(0);
    kmutex_unlock(&fs);
    puts("PASS: boot ownership, context checks, rank order, owner-only release");
}

static void handoff_on_block(void) {
    Thread *waiter = current;
    CHECK(fs.head == waiter && fs.waiters == 1);
    as_thread(0);
    kmutex_unlock(&fs);
    CHECK(kmutex_thread_busy(waiter));
    as_thread(2);
    CHECK(!kmutex_trylock(&fs)); /* A newly runnable process cannot barge. */
    current = waiter;
    current->state = THREAD_RUNNING;
}
static void test_resumed_waiter(void) {
    reset();
    as_thread(0);
    kmutex_lock(&fs);
    on_block = handoff_on_block;
    as_thread(1);
    kmutex_lock(&fs); /* Returns through the actual post-sleep ownership check. */
    CHECK(wakes == 1 && fs.owner == current && fs.waiters == 0);
    CHECK(fs.acquisitions == 2 && fs.contentions == 1);
    PANICS(kmutex_thread_cleanup(current));
    kmutex_unlock(&fs);
    CHECK(!kmutex_thread_busy(current) && !fs.locked);
    puts("PASS: sleep/resume, ownership-before-wakeup, no barging, owner kill guard");
}

static void test_fifo_and_cancellation(void) {
    reset();
    as_thread(0);
    kmutex_lock(&fs);
    queue_thread(1);
    queue_thread(2);
    queue_thread(3);
    CHECK(fs.waiters == 3 && fs.head == &threads[1] && fs.tail == &threads[3]);
    /* Cancel a middle waiter using production cleanup, then prove FIFO handoff. */
    kmutex_thread_cleanup(&threads[2]);
    CHECK(fs.waiters == 2 && !threads[2].kernel_waiting);
    as_thread(0);
    kmutex_unlock(&fs);
    CHECK(fs.owner == &threads[1] && fs.waiters == 1);
    as_thread(1);
    kmutex_unlock(&fs);
    CHECK(fs.owner == &threads[3] && !fs.head && !fs.tail && !fs.waiters);
    as_thread(3);
    kmutex_unlock(&fs);
    /* Cancel both ends, including the final waiter, without a stale tail. */
    as_thread(0);
    kmutex_lock(&fs);
    queue_thread(1);
    queue_thread(2);
    queue_thread(3);
    kmutex_thread_cleanup(&threads[1]);
    kmutex_thread_cleanup(&threads[3]);
    CHECK(fs.head == &threads[2] && fs.tail == &threads[2] && fs.waiters == 1);
    kmutex_thread_cleanup(&threads[2]);
    CHECK(!fs.head && !fs.tail && !fs.waiters);
    as_thread(0);
    kmutex_unlock(&fs);
    puts("PASS: FIFO handoff and head/middle/tail/final waiter cancellation");
}

static void nested_handoff(void) {
    Thread *waiter = current;
    CHECK(waiter->kernel_locks == &fs && waiter->kernel_waiting == &vm);
    CHECK(kmutex_thread_busy(waiter));
    as_thread(0);
    kmutex_unlock(&vm);
    CHECK(waiter->kernel_locks == &vm && vm.owner_previous == &fs);
    current = waiter;
    current->state = THREAD_RUNNING;
}
static void test_nested_wait_and_user_queue(void) {
    reset();
    as_thread(0);
    kmutex_lock(&vm);
    as_thread(1);
    kmutex_lock(&fs);
    on_block = nested_handoff;
    kmutex_lock(&vm);
    kmutex_unlock(&vm);
    CHECK(current->kernel_locks == &fs);
    kmutex_unlock(&fs);
    CHECK(!current->kernel_locks);
    as_thread(0);
    kmutex_lock(&fs);
    as_thread(1);
    current->sync_wait_handle = 123;
    current->sync_next = &threads[4];
    PANICS(kmutex_lock(&fs));
    CHECK(!fs.waiters && current->sync_wait_handle == 123 && current->sync_next == &threads[4]);
    current->sync_wait_handle = 0;
    as_thread(0);
    kmutex_unlock(&fs);
    puts("PASS: nested ownership across wait and isolation from user sync queues");
}

int main(void) {
    test_context_and_order();
    test_resumed_waiter();
    test_fifo_and_cancellation();
    test_nested_wait_and_user_queue();
    puts("PASS: kernel mutex host tests; RV64/QEMU verification remains pending");
    return 0;
}
