#ifndef WUWAOS_USER_API_H
#define WUWAOS_USER_API_H
#include "workstate_info.h"

long write(long fd, const char *buffer, unsigned long length);
long getpid(void);
/* WUWAOS synchronization handles; not POSIX pthread/sem APIs. */
long mutex_create(void);
long mutex_lock(long handle);
long mutex_unlock(long handle);
long semaphore_create(long initial);
long semaphore_wait(long handle);
long semaphore_post(long handle);
long sync_destroy(long handle);
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
long exec_arg(long program, const char *argument);
long console_mode(long raw);
long sbrk(long increment);
long vmstat(long frame_limit);
long workstate_snapshot(WorkstateSnapshot *snapshot);
long sleep_ms(long milliseconds);
long console_poll(void);
long chdir(const char *path);
long getcwd(char *buffer, unsigned long capacity);
long fs_list(const char *path, char *buffer, unsigned long capacity);
long fs_mkdir(const char *path);
long fs_touch(const char *path);
long fs_readfile(const char *path, char *buffer, unsigned long capacity);
long fs_writefile(const char *path, const char *buffer, unsigned long length);
long fs_remove(const char *path);
long fs_remove_tree(const char *path);
long fs_copy(const char *source, const char *destination);
_Noreturn void exit(long status);

#endif
