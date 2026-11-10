#ifndef WUWAOS_KMUTEX_H
#define WUWAOS_KMUTEX_H

#include "kernel/proc/thread.h"

/* Strictly increasing ranks prevent AB/BA deadlock. Unlock in reverse order. */
enum { KMUTEX_RANK_FS = 10, KMUTEX_RANK_VM = 20 };

typedef struct KernelMutex {
    const char *name;
    unsigned int rank;
    int locked;
    Thread *owner;
    Thread *head, *tail;
    struct KernelMutex *owner_previous;
    unsigned int waiters;
    uint64_t acquisitions, contentions;
} KernelMutex;

#define KMUTEX_INITIALIZER(label, order) { .name = (label), .rank = (order) }

/* Static lifetime only. Single hart, SIE=0; forbidden inside an IRQ callback.
 * No-thread boot/scheduler callers may acquire an uncontended lock, never sleep.
 * No recursion. The owner retains the lock across a voluntary context switch.
 */
void kmutex_lock(KernelMutex *mutex);
int kmutex_trylock(KernelMutex *mutex);
void kmutex_unlock(KernelMutex *mutex);
int kmutex_thread_busy(const Thread *thread);
void kmutex_thread_cleanup(Thread *thread);
void kmutex_print(const KernelMutex *mutex);

#endif
