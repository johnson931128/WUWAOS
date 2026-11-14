#include "syscall_numbers.h"
#include "user/lib/api.h"
#include "user/lib/print.h"

#define CHECK(test) do { if (!(test)) { user_puts("FAIL: SMP "); \
    user_number(__LINE__); user_puts("\n"); return 1; } } while (0)

static unsigned arrived;
static unsigned maximum;
static unsigned active;

static long rendezvous(void *unused) {
    (void)unused;
    unsigned count = __atomic_add_fetch(&active, 1, __ATOMIC_ACQ_REL);
    unsigned previous = __atomic_load_n(&maximum, __ATOMIC_RELAXED);
    while (previous < count && !__atomic_compare_exchange_n(&maximum, &previous, count, 0,
                                                            __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {}
    __atomic_fetch_add(&arrived, 1, __ATOMIC_RELEASE);
    unsigned long start, now;
    __asm__ volatile("rdtime %0" : "=r"(start));
    do {
        if (__atomic_load_n(&arrived, __ATOMIC_ACQUIRE) == 2) break;
        __asm__ volatile("rdtime %0" : "=r"(now));
    } while (now - start < 350000UL);
    __atomic_sub_fetch(&active, 1, __ATOMIC_ACQ_REL);
    return __atomic_load_n(&arrived, __ATOMIC_ACQUIRE) == 2 ? 0 : 1;
}

long user_main(long unused) {
    (void)unused;
    /* Hardware progress within less than one timer slice proves parallel dispatch. */
    long tid = thread_create(rendezvous, 0);
    CHECK(tid > 0 && rendezvous(0) == 0);
    long status = 1;
    CHECK(thread_join(tid, &status) == tid && status == 0 && maximum == 2);
    user_puts("PASS: concurrent threads in one address space\n");
    for (unsigned i = 0; i < 300000; ++i) CHECK(getpid() > 0);
    user_puts("PASS: syscall loop survived timer slices\n");
    return 0;
}
