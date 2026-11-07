#include "kernel/io/tty.h"

#define INPUT_CAPACITY 1024U
#define OUTPUT_CAPACITY 8192U

static unsigned char input_bytes[INPUT_CAPACITY], output_bytes[OUTPUT_CAPACITY];
static unsigned int input_read, input_write, output_read, output_write;
static int session_active;
static TtyGuiOutputConsumer output_consumer;

void tty_gui_set_output_consumer(TtyGuiOutputConsumer consumer) { output_consumer = consumer; }
int tty_gui_session_active(void) { return session_active; }

void tty_gui_session_begin(void) {
    input_read = input_write = output_read = output_write = 0;
    session_active = 1;
}

void tty_gui_session_end(void) { session_active = 0; }

int tty_gui_input_push(const unsigned char *bytes, size_t length) {
    if (!bytes && length) return -1;
    size_t copied = 0;
    while (copied < length && input_write - input_read < INPUT_CAPACITY)
        input_bytes[input_write++ % INPUT_CAPACITY] = bytes[copied++];
    return (int)copied;
}

int tty_gui_input_read_byte(void) {
    if (input_read == input_write) return -1;
    return input_bytes[input_read++ % INPUT_CAPACITY];
}

static void drain_output(void) {
    if (!output_consumer) { output_read = output_write; return; }
    while (output_read != output_write) {
        const unsigned int available = output_write - output_read;
        const unsigned int until_wrap = OUTPUT_CAPACITY - (output_read % OUTPUT_CAPACITY);
        const unsigned int length = available < until_wrap ? available : until_wrap;
        output_consumer(&output_bytes[output_read % OUTPUT_CAPACITY], length);
        output_read += length;
    }
}

long tty_gui_output_write(const unsigned char *bytes, size_t length) {
    if (!bytes && length) return -1;
    size_t copied = 0;
    while (copied < length) {
        if (output_write - output_read >= OUTPUT_CAPACITY) drain_output();
        output_bytes[output_write++ % OUTPUT_CAPACITY] = bytes[copied++];
    }
    drain_output();
    return (long)copied;
}
