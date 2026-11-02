#ifndef WUWAOS_INPUT_H
#define WUWAOS_INPUT_H
#include <stdint.h>

typedef enum { INPUT_MESSAGE_POINTER, INPUT_MESSAGE_BUTTON, INPUT_MESSAGE_KEY, INPUT_MESSAGE_RESET } InputMessageType;
typedef struct {
    uint8_t type;
    uint16_t code;
    int32_t value;
    uint32_t x, y;
} InputMessage;

int input_probe(const void *fdt);
unsigned int input_device_count(void);
uintptr_t input_mmio_base(unsigned int index);
int input_init(void);
int input_keyboard_ready(void);
void input_poll(void);
int input_next_message(InputMessage *message);
unsigned int input_overflow_count(void);

#endif
