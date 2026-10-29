#include "api.h"
#include "shell.h"
#include "print.h"
#include "syscall_numbers.h"
#include "vm_limits.h"
#include "workstate.h"
#include "sync_demo.h"
#include <limits.h>

static int equal(const char *first, const char *second) {
    while (*first && *first == *second) {
        ++first;
        ++second;
    }
    return *first == *second;
}

static int tokenize(char *cursor, char *words[4]) {
    int count = 0;
    while (*cursor) {
        if (*cursor == ' ' || *cursor == '\t' || *cursor == '\n') {
            ++cursor;
            continue;
        }
        if (count == 4) {
            return -1;
        }
        if (*cursor == '|') {
            words[count++] = "|";
            ++cursor;
            continue;
        }
        words[count++] = cursor;
        while (*cursor && *cursor != ' ' && *cursor != '\t' &&
               *cursor != '\n' && *cursor != '|') {
            ++cursor;
        }
        if (*cursor == '|') {
            *cursor++ = '\0';
            if (count == 4) {
                return -1;
            }
            words[count++] = "|";
        } else if (*cursor) {
            *cursor++ = '\0';
        }
    }
    return count;
}

static int positive_number(const char *text, long *result) {
    long value = 0;
    if (*text == '\0') {
        return 0;
    }
    while (*text) {
        const int digit = *text++ - '0';
        if (digit < 0 || digit > 9 || value > (LONG_MAX - digit) / 10) {
            return 0;
        }
        value = value * 10 + digit;
    }
    *result = value;
    return value > 0;
}

static void result_line(const char *label, long result) {
    user_puts(label);
    user_number(result);
    user_puts("\n");
}

static void wait_child(long pid) {
    long status = 0;
    const long result = wait(pid, &status);
    if (result < 0) {
        result_line("wait error: ", result);
    } else {
        user_puts("collected PID=");
        user_number(result);
        result_line(" status=", status);
    }
}

static void run_pipeline(void) {
    long pids[2];
    const long result = pipeline(pids);
    if (result < 0) {
        result_line("pipeline error: ", result);
        return;
    }
    wait_child(pids[0]);
    wait_child(pids[1]);
    user_puts("pipeline complete\n");
}

static long fork_sequence;

static void fork_shell(void) {
    volatile long marker = 100;
    ++fork_sequence;
    const long child = fork();
    if (child == 0) {
        fork_sequence += 100;
        ++marker;
        user_puts("[fork child] PID=");
        user_number(getpid());
        user_puts(" return=0 data=");
        user_number(fork_sequence);
        result_line(" stack=", marker);
        exit(0);
    }
    result_line("[fork parent] child PID/result=", child);
    /* Let the child modify its private pages before showing the parent's copy. */
    if (child > 0) {
        yield();
    }
    user_puts("[fork parent] data=");
    user_number(fork_sequence);
    result_line(" stack=", marker);
}

static void run_program(const char *name) {
    const long program = equal(name, "worker") ? PROGRAM_WORKER :
                         equal(name, "quick") ? PROGRAM_QUICK :
                         equal(name, "counter") ? PROGRAM_COUNTER : -1;
    if (program < 0) {
        user_puts("Usage: run worker|quick|counter\n");
        return;
    }
    const long child = fork();
    if (child == 0) {
        user_puts("[run child] fork returned 0; PID=");
        user_number(getpid());
        user_puts("; calling exec\n");
        result_line("exec error: ", exec(program));
        exit(127);
    }
    result_line("[run parent] child PID/result=", child);
}

static void run_fan(const char *path) {
    const long child = fork();
    if (child == 0) {
        result_line("fan exec error: ", exec_arg(PROGRAM_FAN, path));
        exit(127);
    }
    if (child < 0) {
        result_line("fan fork error: ", child);
        return;
    }
    wait_child(child);
    /* Also recover the terminal if the editor exited through a user trap. */
    user_puts("\033[0m\033[?25h\033[?1049l");
}

static volatile unsigned char *memory;
static long memory_pages;

static int nonnegative(const char *text, long *value) {
    if (equal(text, "0")) { *value = 0; return 1; }
    return positive_number(text, value);
}

/* Interactive memory tool: allocation, filling and inspection, without assertions. */
static void memory_command(int count, char *words[4]) {
    long page, value;
    if (count == 2 && equal(words[1], "free")) {
        if (memory_pages == 0) { user_puts("No memory allocation.\n"); return; }
        const long result = sbrk(-memory_pages * (long)VM_PAGE_BYTES);
        if (result < 0) { result_line("sbrk error: ", result); return; }
        memory = 0;
        memory_pages = 0;
        user_puts("Heap allocation released.\n");
        return;
    }
    if (count == 3 && equal(words[1], "alloc") && positive_number(words[2], &page)) {
        if (memory_pages != 0) { user_puts("Use mem free before allocating again.\n"); return; }
        if ((unsigned long)page > USER_HEAP_PAGES) { user_puts("Maximum: 256 pages.\n"); return; }
        const long address = sbrk(page * (long)VM_PAGE_BYTES);
        if (address < 0) { result_line("sbrk error: ", address); return; }
        memory = (volatile unsigned char *)(unsigned long)address;
        memory_pages = page;
        result_line("Reserved heap pages (allocated on first touch): ", page);
        return;
    }
    if (count >= 3 && nonnegative(words[2], &page) && page < memory_pages) {
        volatile unsigned char *bytes = memory + (unsigned long)page * VM_PAGE_BYTES;
        if (count == 4 && equal(words[1], "write") && nonnegative(words[3], &value) && value <= 255) {
            for (unsigned long i = 0; i < VM_PAGE_BYTES; ++i) bytes[i] = (unsigned char)value;
            user_puts("Page "); user_number(page); result_line(" filled with byte ", value);
            return;
        }
        if (count == 3 && equal(words[1], "read")) {
            user_puts("Page "); user_number(page);
            user_puts(" bytes[0,2048,4095] = ");
            user_number(bytes[0]); user_puts(" ");
            user_number(bytes[2048]); user_puts(" ");
            result_line("", bytes[4095]);
            return;
        }
    }
    user_puts("Usage: mem alloc <1..256> | mem write <page> <0..255> | mem read <page> | mem free\n");
}

static void show_cwd(void) {
    char cwd[128];
    long result = getcwd(cwd, sizeof(cwd));
    if (result < 0) result_line("pwd error: ", result);
    else { user_puts(cwd); user_puts("\n"); }
}

static void change_directory(const char *path) {
    char expanded[128];
    int print = equal(path, "-");
    if (path[0] == '~' && (path[1] == 0 || path[1] == '/')) {
        const char *home = "/home/user";
        unsigned long n = 0;
        while (*home) expanded[n++] = *home++;
        ++path;
        while (*path) {
            if (n + 1 >= sizeof(expanded)) { user_puts("cd error: path too long\n"); return; }
            expanded[n++] = *path++;
        }
        expanded[n] = 0;
        path = expanded;
    }
    long result = chdir(path);
    if (result < 0) result_line("cd error: ", result);
    else if (print) show_cwd();
}

static void sync_command(int count, char *words[4]) {
    long argument = 0, result;
    if (count == 3 && equal(words[1], "mutex") && equal(words[2], "create")) result = mutex_create();
    else if (count == 4 && nonnegative(words[3], &argument)) {
        if (equal(words[1], "mutex") && equal(words[2], "lock")) result = mutex_lock(argument);
        else if (equal(words[1], "mutex") && equal(words[2], "unlock")) result = mutex_unlock(argument);
        else if (equal(words[1], "sem") && equal(words[2], "create")) result = semaphore_create(argument);
        else if (equal(words[1], "sem") && equal(words[2], "wait")) result = semaphore_wait(argument);
        else if (equal(words[1], "sem") && equal(words[2], "post")) result = semaphore_post(argument);
        else { user_puts("Invalid sync command.\n"); return; }
    } else if (count == 3 && equal(words[1], "destroy") && positive_number(words[2], &argument)) {
        result = sync_destroy(argument);
    } else { user_puts("Usage: sync mutex create|lock <id>|unlock <id>; sync sem create <n>|wait <id>|post <id>; sync destroy <id>\n"); return; }
    result_line("sync result=", result);
}

/* Run one synchronization operation in a separate child process. */
static void sync_background(int count, char *words[4]) {
    long id;
    if (count != 4 || !positive_number(words[3], &id)) {
        user_puts("Usage: syncbg sem wait|post <id>; syncbg mutex hold <id>\n");
        return;
    }
    int hold = equal(words[1], "mutex") && equal(words[2], "hold");
    int sem_wait = equal(words[1], "sem") && equal(words[2], "wait");
    int sem_post = equal(words[1], "sem") && equal(words[2], "post");
    if (!hold && !sem_wait && !sem_post) { user_puts("Invalid syncbg command.\n"); return; }
    long child = fork();
    if (child == 0) {
        long result = hold ? mutex_lock(id) : sem_wait ? semaphore_wait(id) : semaphore_post(id);
        user_puts("[syncbg] PID="); user_number(getpid());
        result_line(" result=", result);
        if (hold && result == 0) {
            user_puts("[syncbg] holding mutex; wakeup 7 releases it.\n");
            sleep(7);
            result_line("[syncbg] unlock result=", mutex_unlock(id));
        }
        exit(result < 0 ? result : 0);
    }
    result_line("syncbg PID/result=", child);
}

static void help(void) {
    user_puts("help                            Show this command list.\n"
              "ps                              List processes and the RR ready queue.\n"
              "demo sync                       Observe mutex and semaphore.\n"
              "workstate                       Live CPU/memory monitor; q returns.\n"
              "pcb <pid>                       Show one process control block.\n"
              "spawn worker                    Create a worker; wake it with wakeup 7.\n"
              "spawn quick                     Create a process that exits with code 40.\n"
              "fork                            Copy this shell; wait <pid> to collect.\n"
              "run worker                      Fork/exec worker; wakeup 7, then wait.\n"
              "run quick                       Fork/exec quick; wait <pid> to collect.\n"
              "run counter                     Fork/exec counter; wakeup 9, then wait.\n"
              "yield                           Let the next ready process run.\n"
              "sync mutex create|lock <id>|unlock <id>  Manage a mutex.\n"
              "sync sem create <n>|wait <id>|post <id> Manage a semaphore.\n"
              "sync destroy <id>               Destroy an unused sync object.\n"
              "syncbg sem wait|post <id>       Run semaphore operation in child.\n"
              "syncbg mutex hold <id>          Hold in child until wakeup 7.\n"
              "wakeup <channel>                Wake processes waiting on an event.\n"
              "wait <pid>                      Wait for and collect a direct child.\n"
              "kill <pid>                      Terminate a direct child process.\n"
              "trace on                        Enable child process state messages.\n"
              "trace off                       Disable child process state messages.\n"
              "writer | reader                 Run the built-in foreground byte pipe.\n"
              "\n"
              "pwd                             Show current directory.\n"
              "cd [path]                       Change directory; - returns to previous.\n"
              "ls <path>                       List directory entries.\n"
              "ls                              List the current directory.\n"
              "mkdir <path>                    Create a directory.\n"
              "touch <path>                    Create a file; keep existing contents.\n"
              "cat <path>                      Display up to 2048 bytes of a file.\n"
              "write <path> <text>             Overwrite a file with one text token.\n"
              "rm <path>                       Remove file or confirm directory tree.\n"
              "cp <source> <destination>       Copy a regular file.\n"
              "cls                             Clear screen and scrollback.\n"
              "fan <path>                      Edit text; Ctrl+O save, Ctrl+X exit.\n"
              "\n"
              "vmstat                          Show page faults, FIFO and swap usage.\n"
              "vmstat limit <frames>           Set the global resident limit (min 8).\n"
              "mem alloc <pages>               Reserve 1..256 heap pages on demand.\n"
              "mem write <page> <byte>         Fill a heap page with byte 0..255.\n"
              "mem read <page>                 Show the first, middle and last bytes.\n"
              "mem free                        Release the heap allocation.\n"
              "exit                            End the shell; Ctrl+A then X quits QEMU.\n"
              "\n"
              "<path> may be absolute or relative; <page> is a zero-based page index.\n"
              "Files persist on the QEMU disk. Scheduling uses timer-driven RR.\n"
              "Host commands are not available here.\n");
}

static void filesystem_command(const char *command, const char *path, const char *text) {
    if (equal(command, "ls")) {
        char output[512];
        const long length = fs_list(path, output, sizeof(output));
        if (length < 0) result_line("ls error: ", length);
        else if (user_write_all(1, output, (unsigned long)length) < 0) exit(91);
    } else if (equal(command, "mkdir")) {
        result_line("mkdir result=", fs_mkdir(path));
    } else if (equal(command, "touch")) {
        result_line("touch result=", fs_touch(path));
    } else if (equal(command, "rm")) {
        long result = fs_remove(path);
        if (result == -21) {
            char reply[8];
            user_puts("WARNING: Delete directory ");
            user_puts(path);
            user_puts(" and ALL contents? [Y/N] ");
            long length = read(0, reply, sizeof(reply) - 1);
            if (length == 2 && reply[1] == '\n' && (reply[0] == 'Y' || reply[0] == 'y')) {
                result = fs_remove_tree(path);
            } else {
                user_puts("Cancelled.\n");
                return;
            }
        }
        result_line("rm result=", result);
    } else if (equal(command, "cat")) {
        char output[2048];
        const long length = fs_readfile(path, output, sizeof(output));
        if (length < 0) result_line("cat error: ", length);
        else {
            if (user_write_all(1, output, (unsigned long)length) < 0) exit(92);
            if (length > 0 && output[length - 1] != '\n') user_puts("\n");
            if (length == (long)sizeof(output)) user_puts("[cat: display limit reached]\n");
        }
    } else if (equal(command, "write")) {
        unsigned long length = 0;
        while (text[length]) ++length;
        result_line("write bytes=", fs_writefile(path, text, length));
    }
}

static int command(int count, char *words[4]) {
    if (count == 0) {
        return 1;
    }
    if (equal(words[0], "syncbg")) {
        sync_background(count, words);
        return 1;
    }
    if (equal(words[0], "sync")) {
        sync_command(count, words);
        return 1;
    }
    if (equal(words[0], "mem")) {
        memory_command(count, words);
        return 1;
    }
    if (equal(words[0], "vmstat")) {
        long limit;
        if (count == 1) vmstat(0);
        else if (count == 3 && equal(words[1], "limit") && positive_number(words[2], &limit)) {
            const long result = vmstat(limit);
            if (result < 0) result_line("vmstat error: ", result);
        } else user_puts("Usage: vmstat [limit <frames>]\n");
        return 1;
    }
    if (count == 3 && equal(words[0], "writer") && equal(words[1], "|") &&
        equal(words[2], "reader")) {
        run_pipeline();
        return 1;
    }
    if (count == 1) {
        if (equal(words[0], "help")) {
            help();
        } else if (equal(words[0], "ps")) {
            ps();
        } else if (equal(words[0], "pwd")) {
            show_cwd();
        } else if (equal(words[0], "cd")) {
            change_directory("~");
        } else if (equal(words[0], "ls")) {
            filesystem_command("ls", ".", "");
        } else if (equal(words[0], "cls")) {
            user_puts("\033[2J\033[3J\033[H");
        } else if (equal(words[0], "workstate")) {
            workstate_main();
        } else if (equal(words[0], "fork")) {
            fork_shell();
        } else if (equal(words[0], "yield")) {
            yield();
        } else if (equal(words[0], "exit")) {
            return 0;
        } else {
            user_puts("Unknown command. Type help.\n");
        }
        return 1;
    }
    if (count == 2) {
        if (equal(words[0], "demo") && equal(words[1], "sync")) {
            sync_demo();
            return 1;
        }
        if (equal(words[0], "cd")) {
            change_directory(words[1]);
            return 1;
        }
        if (equal(words[0], "fan")) {
            run_fan(words[1]);
            return 1;
        }
        if (equal(words[0], "run")) {
            run_program(words[1]);
            return 1;
        }
        if (equal(words[0], "spawn")) {
            const long program = equal(words[1], "worker") ? PROGRAM_WORKER :
                                 equal(words[1], "quick") ? PROGRAM_QUICK : -1;
            result_line("spawn PID/result=", spawn(program));
            return 1;
        }
        if (equal(words[0], "trace")) {
            if (equal(words[1], "on") || equal(words[1], "off")) {
                trace(equal(words[1], "on"));
            } else {
                user_puts("Usage: trace on|off\n");
            }
            return 1;
        }
        if (equal(words[0], "ls") || equal(words[0], "mkdir") ||
            equal(words[0], "touch") || equal(words[0], "cat") || equal(words[0], "rm")) {
            filesystem_command(words[0], words[1], "");
            return 1;
        }
        long value;
        if (positive_number(words[1], &value)) {
            if (equal(words[0], "pcb")) {
                const long result = pcb(value);
                if (result < 0) {
                    result_line("pcb error: ", result);
                }
                return 1;
            }
            if (equal(words[0], "wakeup")) {
                result_line("woken=", wakeup(value));
                return 1;
            }
            if (equal(words[0], "wait")) {
                wait_child(value);
                return 1;
            }
            if (equal(words[0], "kill")) {
                result_line("kill result=", kill(value));
                return 1;
            }
        }
    }
    if (count == 3 && equal(words[0], "write")) {
        filesystem_command(words[0], words[1], words[2]);
        return 1;
    }
    if (count == 3 && equal(words[0], "cp")) {
        result_line("cp bytes=", fs_copy(words[1], words[2]));
        return 1;
    }
    user_puts("Invalid command or argument. Type help.\n");
    return 1;
}

long shell_main(void) {
    user_puts("WUWAOS user shell. Type help. State tracing is on.\n");
    for (;;) {
        char line[128];
        char *words[4];
        /* Built-in worker yields once before sleeping; let it reach that point
           before drawing a fresh prompt. This is still cooperative scheduling. */
        yield();
        yield();
        char cwd[128];
        user_puts("wuwaos");
        if (getcwd(cwd, sizeof(cwd)) == 0) user_puts(cwd);
        else user_puts("?");
        user_puts("> ");
        const long length = read(0, line, sizeof(line) - 1);
        if (length < 0) {
            user_puts("Input rejected: line too long or invalid buffer.\n");
            continue;
        }
        line[length] = '\0';
        const int count = tokenize(line, words);
        if (count < 0) {
            user_puts("Too many command tokens.\n");
        } else if (!command(count, words)) {
            user_puts("Shell exiting.\n");
            return 0;
        }
    }
}
