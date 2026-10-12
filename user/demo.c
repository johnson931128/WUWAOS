#include "api.h"

/* The first real user program: get its PID, print, then exit. */
long user_main(void) {
    char message[] = "[user] Hello from PID 0\n";
    const long pid = getpid();
    if (pid < 1 || pid > 9) {
        return 1;
    }
    message[22] = (char)('0' + pid);
    const long written = write(1, message, sizeof(message) - 1);
    return written == (long)(sizeof(message) - 1) ? 0 : 2;
}
