#ifndef WUWAOS_IRQ_H
#define WUWAOS_IRQ_H

#include <stdint.h>

/* Bounded to one PLIC priority page; IRQ 0 is the no-pending sentinel. */
#define IRQ_SOURCE_LIMIT 1024U
/* Return nonzero after device ACK, zero for a spurious claim; never block here. */
typedef int (*IrqService)(void *device);

int irq_probe(const void *fdt, unsigned long hart_id);
unsigned int irq_mmio_count(void);
uintptr_t irq_mmio_base(unsigned int index);
/* Boot-time registration only, before irq_start enables external interrupts. */
int irq_register_mmio(uintptr_t base, IrqService service, void *device, const char *name);
void irq_start(void);
void irq_dispatch(void);
int irq_in_handler(void);
/* Called from ordinary kernel diagnostic paths with interrupts disabled. */
void irq_print_stats(void);

#endif
