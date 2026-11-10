#ifndef WUWAOS_UI_INPUT_KEYMAP_H
#define WUWAOS_UI_INPUT_KEYMAP_H
#include <stddef.h>
#include <stdint.h>

void ui_keymap_reset(void);
size_t ui_keymap_translate(uint16_t code, int pressed, unsigned char output[3]);

#endif
