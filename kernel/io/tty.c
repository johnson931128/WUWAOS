#include "kernel/io/tty.h"
#include "drivers/uart/uart.h"
#include "syscall_numbers.h"

#define TTY_ENDPOINT_LIMIT 16U
#define INPUT_CAPACITY 1024U
#define OUTPUT_CAPACITY 8192U
#define TTY_UART_SLOT 0U

typedef struct {
    uint32_t generation;
    unsigned used : 1;
    unsigned active : 1;
    unsigned uart : 1;
    unsigned skip_lf : 1;
    unsigned references;
    long raw_owner;
    unsigned char input[INPUT_CAPACITY];
    unsigned char output[OUTPUT_CAPACITY];
    uint32_t input_read, input_write, output_read, output_write;
    TtyOutputConsumer consumer;
    void *consumer_context;
    KernelEventSource input_event;
} TtyEndpoint;

static TtyEndpoint endpoints[TTY_ENDPOINT_LIMIT];
static int initialized;

static TtyId make_id(unsigned slot, uint32_t generation) {
    return ((uint64_t)generation << 32) | (uint64_t)(slot + 1U);
}

static TtyEndpoint *get_endpoint(TtyId id) {
    const uint32_t encoded_slot = (uint32_t)id;
    const uint32_t generation = (uint32_t)(id >> 32);
    if (encoded_slot == 0 || encoded_slot > TTY_ENDPOINT_LIMIT || generation == 0) return 0;
    TtyEndpoint *endpoint = &endpoints[encoded_slot - 1U];
    return endpoint->used && endpoint->generation == generation ? endpoint : 0;
}

static void uart_output(TtyId id, void *context, const unsigned char *bytes, size_t length) {
    (void)id; (void)context;
    for (size_t i = 0; i < length; ++i) {
        if (bytes[i] == '\n') uart_putc_raw('\r');
        uart_putc_raw((char)bytes[i]);
    }
}

void tty_init(void) {
    if (initialized) return;
    initialized = 1;
    TtyEndpoint *uart = &endpoints[TTY_UART_SLOT];
    *uart = (TtyEndpoint){
        .generation = 1,
        .used = 1,
        .active = 1,
        .uart = 1,
        .references = 1,
        .consumer = uart_output
    };
    (void)event_source_init(&uart->input_event, uart->generation);
}

void tty_poll(void) {
    tty_init();
    TtyEndpoint *uart = &endpoints[TTY_UART_SLOT];
    size_t copied = 0;
    while (uart->input_write - uart->input_read < INPUT_CAPACITY) {
        const int character = uart_getc_raw();
        if (character < 0) break;
        uart->input[uart->input_write++ % INPUT_CAPACITY] = (unsigned char)character;
        ++copied;
    }
    if (copied) event_source_notify(&uart->input_event, 0);
}

TtyId tty_uart_id(void) {
    tty_init();
    return make_id(TTY_UART_SLOT, endpoints[TTY_UART_SLOT].generation);
}

TtyId tty_session_create(TtyOutputConsumer consumer, void *context) {
    tty_init();
    for (unsigned slot = TTY_UART_SLOT + 1U; slot < TTY_ENDPOINT_LIMIT; ++slot) {
        TtyEndpoint *endpoint = &endpoints[slot];
        if (endpoint->used || endpoint->generation == UINT32_MAX || endpoint->input_event.retired) continue;
        uint32_t generation = endpoint->generation + 1U;
        const KernelEventSource previous_event = endpoint->input_event;
        *endpoint = (TtyEndpoint){
            .generation = generation,
            .used = 1,
            .active = 1,
            .references = 1,
            .consumer = consumer,
            .consumer_context = context,
            .input_event = previous_event
        };
        if (event_source_init(&endpoint->input_event, generation) < 0) {
            *endpoint = (TtyEndpoint){ .generation = generation, .input_event = previous_event };
            continue;
        }
        return make_id(slot, generation);
    }
    return TTY_ID_INVALID;
}

int tty_session_active(TtyId id) {
    tty_init();
    TtyEndpoint *endpoint = get_endpoint(id);
    return endpoint && endpoint->active;
}

KernelEventSource *tty_input_event(TtyId id, uint32_t *generation) {
    tty_init();
    TtyEndpoint *endpoint = get_endpoint(id);
    if (!endpoint || !endpoint->active) return 0;
    if (generation) *generation = endpoint->input_event.generation;
    return &endpoint->input_event;
}

int tty_retain(TtyId id) {
    tty_init();
    TtyEndpoint *endpoint = get_endpoint(id);
    if (!endpoint || !endpoint->active || endpoint->references == UINT32_MAX) return -1;
    ++endpoint->references;
    return 0;
}

void tty_release(TtyId id) {
    tty_init();
    TtyEndpoint *endpoint = get_endpoint(id);
    if (!endpoint || endpoint->references == 0) return;
    if (endpoint->uart && endpoint->references == 1) return;
    if (--endpoint->references == 0) {
        const uint32_t generation = endpoint->generation;
        event_source_close(&endpoint->input_event, ERR_IO);
        const KernelEventSource input_event = endpoint->input_event;
        *endpoint = (TtyEndpoint){ .generation = generation, .input_event = input_event };
    }
}

static void drain_output(TtyId id, TtyEndpoint *endpoint) {
    if (!endpoint->consumer) return;
    while (endpoint->output_read != endpoint->output_write) {
        const uint32_t available = endpoint->output_write - endpoint->output_read;
        const uint32_t until_wrap = OUTPUT_CAPACITY - (endpoint->output_read % OUTPUT_CAPACITY);
        const uint32_t length = available < until_wrap ? available : until_wrap;
        TtyOutputConsumer consumer = endpoint->consumer;
        void *context = endpoint->consumer_context;
        endpoint->output_read += length;
        consumer(id, context, &endpoint->output[(endpoint->output_read - length) % OUTPUT_CAPACITY], length);
        if (get_endpoint(id) != endpoint || !endpoint->active ||
            endpoint->consumer != consumer || endpoint->consumer_context != context) return;
    }
}

int tty_set_output_consumer(TtyId id, TtyOutputConsumer consumer, void *context) {
    tty_init();
    TtyEndpoint *endpoint = get_endpoint(id);
    if (!endpoint || !endpoint->active) return -1;
    endpoint->consumer = consumer;
    endpoint->consumer_context = context;
    drain_output(id, endpoint);
    return 0;
}

void tty_unset_output_consumer(TtyId id) {
    tty_init();
    TtyEndpoint *endpoint = get_endpoint(id);
    if (!endpoint) return;
    endpoint->consumer = 0;
    endpoint->consumer_context = 0;
}

int tty_session_close(TtyId id) {
    tty_init();
    TtyEndpoint *endpoint = get_endpoint(id);
    if (!endpoint || endpoint->uart) return -1;
    if (!endpoint->active) return 0;
    drain_output(id, endpoint);
    endpoint = get_endpoint(id);
    if (!endpoint || endpoint->uart) return -1;
    endpoint->active = 0;
    endpoint->raw_owner = 0;
    endpoint->skip_lf = 0;
    endpoint->input_read = endpoint->input_write;
    endpoint->consumer = 0;
    endpoint->consumer_context = 0;
    event_source_close(&endpoint->input_event, ERR_IO);
    return 0;
}

int tty_input_push(TtyId id, const unsigned char *bytes, size_t length) {
    tty_init();
    TtyEndpoint *endpoint = get_endpoint(id);
    if (!endpoint || !endpoint->active || (!bytes && length)) return -1;
    size_t copied = 0;
    while (copied < length && endpoint->input_write - endpoint->input_read < INPUT_CAPACITY)
        endpoint->input[endpoint->input_write++ % INPUT_CAPACITY] = bytes[copied++];
    if (copied) event_source_notify(&endpoint->input_event, 0);
    return (int)copied;
}

int tty_input_read_byte(TtyId id) {
    tty_init();
    TtyEndpoint *endpoint = get_endpoint(id);
    if (!endpoint || !endpoint->active) return TTY_READ_CLOSED;
    if (endpoint->input_read == endpoint->input_write) return -1;
    return endpoint->input[endpoint->input_read++ % INPUT_CAPACITY];
}

long tty_output_write(TtyId id, const unsigned char *bytes, size_t length) {
    tty_init();
    TtyEndpoint *endpoint = get_endpoint(id);
    if (!endpoint || !endpoint->active) return ERR_BAD_FD;
    if (!bytes && length) return ERR_INVALID;
    size_t copied = 0;
    while (copied < length) {
        if (endpoint->output_write - endpoint->output_read >= OUTPUT_CAPACITY) {
            drain_output(id, endpoint);
            endpoint = get_endpoint(id);
            if (!endpoint || !endpoint->active) return copied ? (long)copied : ERR_BAD_FD;
            if (endpoint->output_write - endpoint->output_read >= OUTPUT_CAPACITY) break;
        }
        size_t available = OUTPUT_CAPACITY - (endpoint->output_write - endpoint->output_read);
        size_t until_wrap = OUTPUT_CAPACITY - (endpoint->output_write % OUTPUT_CAPACITY);
        size_t batch = length - copied;
        if (batch > available) batch = available;
        if (batch > until_wrap) batch = until_wrap;
        for (size_t i = 0; i < batch; ++i)
            endpoint->output[(endpoint->output_write + i) % OUTPUT_CAPACITY] = bytes[copied + i];
        endpoint->output_write += (uint32_t)batch;
        copied += batch;
        drain_output(id, endpoint);
        endpoint = get_endpoint(id);
        if (!endpoint || !endpoint->active) return copied ? (long)copied : ERR_BAD_FD;
    }
    return copied || length == 0 ? (long)copied : ERR_IO;
}

long tty_raw_owner(TtyId id) {
    tty_init();
    TtyEndpoint *endpoint = get_endpoint(id);
    return endpoint && endpoint->active ? endpoint->raw_owner : ERR_BAD_FD;
}

long tty_set_raw_owner(TtyId id, long pid, int enabled) {
    tty_init();
    TtyEndpoint *endpoint = get_endpoint(id);
    if (!endpoint || !endpoint->active || pid <= 0 || (enabled != 0 && enabled != 1)) return ERR_BAD_FD;
    if (endpoint->raw_owner != 0 && endpoint->raw_owner != pid) return ERR_BUSY;
    const long next_owner = enabled ? pid : 0;
    if (endpoint->raw_owner != next_owner) {
        endpoint->raw_owner = next_owner;
        event_source_notify(&endpoint->input_event, 0);
    }
    return 0;
}

void tty_release_raw_owner(long pid) {
    tty_init();
    for (unsigned i = 0; i < TTY_ENDPOINT_LIMIT; ++i)
        if (endpoints[i].used && endpoints[i].raw_owner == pid) {
            endpoints[i].raw_owner = 0;
            event_source_notify(&endpoints[i].input_event, 0);
        }
}

int tty_filter_crlf(TtyId id, int character, int raw) {
    tty_init();
    TtyEndpoint *endpoint = get_endpoint(id);
    if (!endpoint || !endpoint->active) return -1;
    if (character == '\n' && endpoint->skip_lf) {
        endpoint->skip_lf = 0;
        return 1;
    }
    endpoint->skip_lf = !raw && character == '\r';
    return 0;
}
