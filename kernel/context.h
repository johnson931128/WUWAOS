#ifndef WUWAOS_CONTEXT_H
#define WUWAOS_CONTEXT_H

/* RV64 LP64 callee-saved state at an ordinary C function call boundary. */
#define CTX_RA 0
#define CTX_SP 8
#define CTX_S0 16
#define CTX_S1 24
#define CTX_S2 32
#define CTX_S3 40
#define CTX_S4 48
#define CTX_S5 56
#define CTX_S6 64
#define CTX_S7 72
#define CTX_S8 80
#define CTX_S9 88
#define CTX_S10 96
#define CTX_S11 104
#define CTX_SIZE 112

#ifndef __ASSEMBLER__
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uintptr_t ra, sp;
    uintptr_t saved[12];
} KernelContext;

/* Assembly/C layout contracts, following the existing TrapFrame convention. */
_Static_assert(sizeof(KernelContext) == CTX_SIZE, "KernelContext size");
_Static_assert(offsetof(KernelContext, ra) == CTX_RA, "context ra offset");
_Static_assert(offsetof(KernelContext, sp) == CTX_SP, "context sp offset");
_Static_assert(offsetof(KernelContext, saved) == CTX_S0, "context s0 offset");

/* SIE must be clear. The caller owns page-table and process-state changes. */
void context_switch(KernelContext *old, const KernelContext *next);
#endif
#endif
