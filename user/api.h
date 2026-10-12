#ifndef WUWAOS_USER_API_H
#define WUWAOS_USER_API_H

long write(long fd, const char *buffer, unsigned long length);
long getpid(void);
_Noreturn void exit(long status);

#endif
