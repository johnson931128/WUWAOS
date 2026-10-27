#include "sbi.h"

#define SBI_BASE 0x10UL
#define SBI_TIME 0x54494d45UL

typedef struct { long error, value; } SbiResult;

static SbiResult sbi_call(unsigned long extension, unsigned long function, unsigned long argument) {
    register unsigned long a0 __asm__("a0") = argument;
    register unsigned long a1 __asm__("a1") = 0;
    register unsigned long a6 __asm__("a6") = function;
    register unsigned long a7 __asm__("a7") = extension;
    __asm__ volatile("ecall" : "+r"(a0), "+r"(a1) : "r"(a6), "r"(a7) : "memory");
    return (SbiResult){ (long)a0, (long)a1 };
}

int sbi_timer_available(void) {
    const SbiResult result = sbi_call(SBI_BASE, 3, SBI_TIME);
    return result.error == 0 && result.value != 0;
}

long sbi_set_timer(uint64_t deadline) {
    return sbi_call(SBI_TIME, 0, deadline).error;
}
