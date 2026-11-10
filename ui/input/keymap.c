#include "ui/input/keymap.h"

static unsigned int shift_keys, control_keys;

void ui_keymap_reset(void) { shift_keys = control_keys = 0; }

static unsigned char shifted_ascii(unsigned char character) {
    if (character >= 'a' && character <= 'z') return (unsigned char)(character - 'a' + 'A');
    switch (character) {
    case '1': return '!'; case '2': return '@'; case '3': return '#'; case '4': return '$';
    case '5': return '%'; case '6': return '^'; case '7': return '&'; case '8': return '*';
    case '9': return '('; case '0': return ')'; case '-': return '_'; case '=': return '+';
    case '[': return '{'; case ']': return '}'; case '\\': return '|'; case ';': return ':';
    case '\'': return '"'; case ',': return '<'; case '.': return '>'; case '/': return '?'; case '`': return '~';
    default: return character;
    }
}

static unsigned char key_ascii(uint16_t code) {
    if (code >= 2U && code <= 11U) { static const char row[] = "1234567890"; return row[code - 2U]; }
    if (code >= 16U && code <= 25U) { static const char row[] = "qwertyuiop"; return row[code - 16U]; }
    if (code >= 30U && code <= 38U) { static const char row[] = "asdfghjkl"; return row[code - 30U]; }
    if (code >= 44U && code <= 50U) { static const char row[] = "zxcvbnm"; return row[code - 44U]; }
    switch (code) {
    case 12: return '-'; case 13: return '='; case 26: return '['; case 27: return ']';
    case 39: return ';'; case 40: return '\''; case 41: return '`'; case 43: return '\\';
    case 51: return ','; case 52: return '.'; case 53: return '/'; case 57: return ' ';
    case 14: return '\b'; case 15: return '\t'; case 28: return '\r'; case 1: return 0x1b;
    default: return 0;
    }
}

size_t ui_keymap_translate(uint16_t code, int pressed, unsigned char output[3]) {
    if (!output) return 0;
    if (code == 42U || code == 54U) {
        const unsigned int mask = code == 42U ? 1U : 2U;
        if (pressed) shift_keys |= mask; else shift_keys &= ~mask;
        return 0;
    }
    if (code == 29U || code == 97U) {
        const unsigned int mask = code == 29U ? 1U : 2U;
        if (pressed) control_keys |= mask; else control_keys &= ~mask;
        return 0;
    }
    if (!pressed) return 0;
    if (code == 103U) { output[0] = 0x1b; output[1] = '['; output[2] = 'A'; return 3; }
    if (code == 108U) { output[0] = 0x1b; output[1] = '['; output[2] = 'B'; return 3; }
    if (code == 105U) { output[0] = 0x1b; output[1] = '['; output[2] = 'D'; return 3; }
    if (code == 106U) { output[0] = 0x1b; output[1] = '['; output[2] = 'C'; return 3; }
    unsigned char character = key_ascii(code);
    if (!character) return 0;
    if (control_keys) {
        if (character >= 'a' && character <= 'z') character = (unsigned char)(character - 'a' + 1U);
        else if (character >= 'A' && character <= 'Z') character = (unsigned char)(character - 'A' + 1U);
        else if (character == ' ') character = 0;
    } else if (shift_keys) character = shifted_ascii(character);
    output[0] = character;
    return 1;
}
