#ifndef WUWAOS_TRAP_H
#define WUWAOS_TRAP_H

/* Shared offsets ensure assembly and C describe the same trap frame. */
#define TF_SEPC 256
#define TF_SSTATUS 264
#define TF_SCAUSE 272
#define TF_STVAL 280
#define TF_SIZE 288

#ifndef __ASSEMBLER__
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint64_t registers[32];
    uint64_t sepc;
    uint64_t sstatus;
    uint64_t scause;
    uint64_t stval;
} TrapFrame;

/* Compile-time layout contracts, not runtime test code. */
_Static_assert(sizeof(TrapFrame) == TF_SIZE, "TrapFrame size");
_Static_assert(offsetof(TrapFrame, sepc) == TF_SEPC, "sepc offset");
_Static_assert(offsetof(TrapFrame, sstatus) == TF_SSTATUS, "sstatus offset");
_Static_assert(offsetof(TrapFrame, scause) == TF_SCAUSE, "scause offset");
_Static_assert(offsetof(TrapFrame, stval) == TF_STVAL, "stval offset");

void trap_init(void);
TrapFrame *trap_handler(TrapFrame *frame);
_Noreturn void user_enter(TrapFrame *frame);
#endif
#endif
