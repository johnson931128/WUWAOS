#include "api.h"
#include "shell.h"
#include "print.h"
#include "syscall_numbers.h"
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
    if (path[0] != '/') {
        user_puts("Usage: fan /absolute/path\n");
        return;
    }
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

static void help(void) {
    user_puts("help | ps | pcb <pid> | spawn worker|quick | yield\n"
              "fork | run worker|quick|counter\n"
              "wakeup <channel> | wait <pid> | kill <pid> | trace on|off\n"
              "writer | reader   (built-in foreground byte pipe)\n"
              "exit              (ends shell; Ctrl+A then X quits QEMU)\n"
              "worker waits on channel 7; wakeup 7, then wait its PID.\n"
              "counter waits on channel 9; run two, then wakeup 9.\n"
              "fork/run leave children for explicit wait <pid> collection.\n"
              "filesystem: ls/mkdir/touch/cat/write <absolute-path>\n"
              "fan /absolute/path: editor; Ctrl+O save, Ctrl+X exit\n"
              "write saves one text token; files persist on the QEMU disk.\n"
              "Only cooperative scheduling; no host commands.\n");
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
            equal(words[0], "touch") || equal(words[0], "cat")) {
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
        user_puts("wuwaos> ");
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
