#ifndef WUWAOS_THREAD_LIMITS_H
#define WUWAOS_THREAD_LIMITS_H

/* 16 processes x at most 8 occupied slots = 128 theoretical; system cap is 32. */
#define THREAD_LIMIT 32
#define PROCESS_THREAD_LIMIT 8
#define THREAD_EXTRA_STACK_SLOTS (PROCESS_THREAD_LIMIT - 1)

#endif
