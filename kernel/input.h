#ifndef WUWAOS_INPUT_H
#define WUWAOS_INPUT_H

#include <stdint.h>

int input_probe(const void *fdt);
uintptr_t input_mmio_base(void);
int input_init(void);
int input_poll(uint32_t *x, uint32_t *y);

#endif
