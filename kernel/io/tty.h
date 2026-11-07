#ifndef WUWAOS_KERNEL_IO_TTY_H
#define WUWAOS_KERNEL_IO_TTY_H

#include <stddef.h>
#include <stdint.h>

typedef uint64_t TtyId;
typedef void (*TtyOutputConsumer)(TtyId id, void *context,
                                 const unsigned char *bytes, size_t length);

#define TTY_ID_INVALID ((TtyId)0)
#define TTY_READ_CLOSED (-2)

void tty_init(void);
TtyId tty_uart_id(void);
TtyId tty_session_create(TtyOutputConsumer consumer, void *context);
int tty_session_active(TtyId id);
int tty_session_close(TtyId id);
int tty_retain(TtyId id);
void tty_release(TtyId id);
int tty_set_output_consumer(TtyId id, TtyOutputConsumer consumer, void *context);
void tty_unset_output_consumer(TtyId id);
int tty_input_push(TtyId id, const unsigned char *bytes, size_t length);
int tty_input_read_byte(TtyId id);
long tty_output_write(TtyId id, const unsigned char *bytes, size_t length);
long tty_raw_owner(TtyId id);
long tty_set_raw_owner(TtyId id, long pid, int enabled);
void tty_release_raw_owner(long pid);
int tty_filter_crlf(TtyId id, int character, int raw);

#endif
