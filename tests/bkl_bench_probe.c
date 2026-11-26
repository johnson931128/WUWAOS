#include "syscall_numbers.h"
#include "user/lib/api.h"
#include "user/lib/print.h"

#define MAX_WORKERS 4
#define DEFAULT_ITERS 20000UL

static unsigned target_workers;
static unsigned long target_iters;
static unsigned ready_workers;
static unsigned start_flag;

static unsigned long parse_uint(const char *s, unsigned long fallback) {
    if (!s || !*s) return fallback;
    unsigned long value = 0;
    for (; *s; ++s) {
        if (*s < '0' || *s > '9') return fallback;
        value = value * 10UL + (unsigned long)(*s - '0');
    }
    return value ? value : fallback;
}

static unsigned long read_time(void) {
    unsigned long ticks;
    __asm__ volatile("rdtime %0" : "=r"(ticks));
    return ticks;
}

static long worker_run(void *unused) {
    (void)unused;
    __atomic_fetch_add(&ready_workers, 1, __ATOMIC_ACQ_REL);
    while (__atomic_load_n(&start_flag, __ATOMIC_ACQUIRE) == 0) {}
    for (unsigned long i = 0; i < target_iters; ++i) {
        if (getpid() <= 0) return 1;
    }
    return 0;
}

long user_main(long argc, const char *const argv[]) {
    unsigned long workers = argc > 1 ? parse_uint(argv[1], 1) : 1;
    if (workers < 1) workers = 1;
    if (workers > MAX_WORKERS) workers = MAX_WORKERS;
    target_workers = (unsigned)workers;
    target_iters = argc > 2 ? parse_uint(argv[2], DEFAULT_ITERS) : DEFAULT_ITERS;

    long tids[MAX_WORKERS];
    for (unsigned i = 1; i < target_workers; ++i) {
        tids[i] = thread_create(worker_run, 0);
        if (tids[i] <= 0) {
            user_puts("FAIL: thread_create\n");
            return 1;
        }
    }

    /* Wait until all spawned workers are ready before releasing the barrier. */
    while (__atomic_load_n(&ready_workers, __ATOMIC_ACQUIRE) + 1U < target_workers) {}
    const unsigned long start = read_time();
    __atomic_store_n(&start_flag, 1, __ATOMIC_RELEASE);

    long self_status = worker_run(0);
    for (unsigned i = 1; i < target_workers; ++i) {
        long status = 1;
        if (thread_join(tids[i], &status) != tids[i] || status != 0) {
            user_puts("FAIL: thread_join\n");
            return 1;
        }
    }
    const unsigned long elapsed = read_time() - start;
    if (self_status != 0) {
        user_puts("FAIL: worker\n");
        return 1;
    }

    user_puts("BKL BENCH RESULT threads=");
    user_number((long)target_workers);
    user_puts(" iters=");
    user_number((long)target_iters);
    user_puts(" total_calls=");
    user_number((long)(target_workers * target_iters));
    user_puts(" ticks=");
    user_number((long)elapsed);
    user_puts("\n");
    return 0;
}
