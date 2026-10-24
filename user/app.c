#include "api.h"
#include "print.h"
#include "syscall_numbers.h"

static long worker(void) {
    user_puts("[worker] PID=");
    user_number(getpid());
    user_puts(" started; yielding, then waiting on channel 7\n");
    yield();
    sleep(7);
    user_puts("[worker] event received; exiting\n");
    return 0;
}

static long writer(void) {
    static const char message[] =
        "Hello through a real pipe: this message is longer than the 16-byte ring buffer.\n";
    return user_write_all(1, message, sizeof(message) - 1) == 0 ? 0 : 32;
}

static long reader(void) {
    char message[128];
    unsigned long length = 0;
    for (;;) {
        const unsigned long capacity = sizeof(message) - length;
        if (capacity == 0) {
            return 33;
        }
        const long copied = read(0, message + length, capacity < 12 ? capacity : 12);
        if (copied < 0) {
            return 34;
        }
        if (copied == 0) {
            break;
        }
        length += (unsigned long)copied;
    }
    user_puts("[reader] ");
    if (user_write_all(1, message, length) < 0) {
        return 35;
    }
    user_puts("[reader] EOF, bytes=");
    user_number((long)length);
    user_puts("\n");
    return 0;
}

static volatile long remaining = 3;

static long counter(void) {
    user_puts("[counter] PID=");
    user_number(getpid());
    user_puts(" waiting on channel 9\n");
    sleep(9);
    while (remaining > 0) {
        user_puts("[counter] PID=");
        user_number(getpid());
        user_puts(" remaining=");
        user_number(remaining);
        user_puts("\n");
        --remaining;
        yield();
    }
    return 0;
}
long user_main(long program) {
    switch (program) {
    case PROGRAM_WORKER: return worker();
    case PROGRAM_QUICK:
        user_puts("[quick] finished\n");
        return 40;
    case PROGRAM_WRITER: return writer();
    case PROGRAM_READER: return reader();
    case PROGRAM_COUNTER: return counter();
    default: return -22;
    }
}
