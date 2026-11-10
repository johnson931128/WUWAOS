#ifndef WUWAOS_GPU_H
#define WUWAOS_GPU_H

#include <stdint.h>

#define GPU_WIDTH 1280U
#define GPU_HEIGHT 720U

int gpu_probe(const void *fdt);
uintptr_t gpu_mmio_base(void);
int gpu_init(void);
uint32_t *gpu_framebuffer(void);
int gpu_present(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
void gpu_poll(void);
int gpu_is_enabled(void);

#endif
