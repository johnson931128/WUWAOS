#ifndef WUWAOS_USER_API_H
#define WUWAOS_USER_API_H

long write(long fd, const char *buffer, unsigned long length);
long getpid(void);
long spawn(long program);
long yield(void);
long sleep(long channel);
long wakeup(long channel);
long wait(long pid, long *status);
long read(long fd, char *buffer, unsigned long length);
long ps(void);
long pcb(long pid);
long kill(long pid);
long pipeline(long pids[2]);
long trace(long enabled);
long fork(void);
long exec(long program);
long fs_list(const char *path, char *buffer, unsigned long capacity);
long fs_mkdir(const char *path);
long fs_touch(const char *path);
long fs_readfile(const char *path, char *buffer, unsigned long capacity);
long fs_writefile(const char *path, const char *buffer, unsigned long length);
_Noreturn void exit(long status);

#endif
