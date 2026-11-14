#include "arch/riscv/sbi.h"

#define SBI_BASE 0x10UL
#define SBI_TIME 0x54494d45UL
#define SBI_HSM 0x48534dUL
#define SBI_RFENCE 0x52464e43UL

typedef struct { long error, value; } SbiResult;

static SbiResult sbi_call(unsigned long extension, unsigned long function, unsigned long argument, unsigned long extra, unsigned long size) {
    register unsigned long a0 __asm__("a0") = argument;
    register unsigned long a1 __asm__("a1") = extra;
    register unsigned long a2 __asm__("a2") = 0;
    register unsigned long a3 __asm__("a3") = size;
    register unsigned long a6 __asm__("a6") = function;
    register unsigned long a7 __asm__("a7") = extension;
    __asm__ volatile("ecall" : "+r"(a0), "+r"(a1) : "r"(a2), "r"(a3), "r"(a6), "r"(a7) : "memory");
    return (SbiResult){ (long)a0, (long)a1 };
}

int sbi_timer_available(void) {
    const SbiResult result = sbi_call(SBI_BASE, 3, SBI_TIME, 0, 0);
    return result.error == 0 && result.value != 0;
}

int sbi_smp_available(void) {
    return sbi_call(SBI_BASE, 3, SBI_HSM, 0, 0).value &&
           sbi_call(SBI_BASE, 3, SBI_RFENCE, 0, 0).value;
}

long sbi_hart_start(unsigned long id, uintptr_t address) {
    return sbi_call(SBI_HSM, 0, id, address, 0).error;
}

long sbi_remote_sfence(unsigned long mask) {
    return sbi_call(SBI_RFENCE, 1, mask, 0, UINT64_MAX).error;
}

long sbi_set_timer(uint64_t deadline) {
    return sbi_call(SBI_TIME, 0, deadline, 0, 0).error;
}
