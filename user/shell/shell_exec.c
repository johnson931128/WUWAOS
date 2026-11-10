#include "user/shell/shell_exec.h"
#include "user/lib/api.h"
#include "user/lib/print.h"
#include "syscall_numbers.h"

#define LINE_BYTES 128
#define STAGE_LIMIT 3
#define ARG_LIMIT 8
#define IO_BYTES 256
#define READ_BYTES 128

typedef struct {
    char *argv[ARG_LIMIT];
    int argc;
    char *input, *output;
    int append;
} Stage;

static int same(const char *a, const char *b) {
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}

static void stderr_text(const char *message) {
    unsigned long length = 0;
    while (message[length]) ++length;
    user_write_all(2, message, length);
}

static void error_number(const char *message, long value) {
    char digits[22];
    unsigned long number = (unsigned long)value, count = 0;
    if (value < 0) number = 0UL - number;
    while (number >= 10) { digits[count++] = (char)('0' + number % 10); number /= 10; }
    digits[count++] = (char)('0' + number);
    stderr_text(message);
    if (value < 0) write(2, "-", 1);
    while (count) write(2, &digits[--count], 1);
    write(2, "\n", 1);
}

static int parse(char *line, Stage stages[STAGE_LIMIT], char storage[LINE_BYTES],
                 int *stage_count, int *has_operator) {
    char source[LINE_BYTES];
    unsigned int n = 0, out = 0;
    while (line[n] && n + 1 < sizeof(source)) { source[n] = line[n]; ++n; }
    source[n] = 0;
    for (int i = 0; i < STAGE_LIMIT; ++i) {
        volatile unsigned char *bytes = (volatile unsigned char *)&stages[i];
        for (unsigned long j = 0; j < sizeof(stages[i]); ++j) bytes[j] = 0;
    }
    int stage = 0, pending = 0;
    *has_operator = 0;
    for (unsigned int i = 0; i < n;) {
        if (source[i] == ' ' || source[i] == '\t' || source[i] == '\r' || source[i] == '\n') { ++i; continue; }
        if (source[i] == '|') {
            *has_operator = 1;
            if (pending || stages[stage].argc == 0 || stage + 1 >= STAGE_LIMIT) return -1;
            ++stage; ++i; continue;
        }
        if (source[i] == '<' || source[i] == '>') {
            *has_operator = 1;
            if (pending) return -1;
            if (source[i] == '<') pending = 1;
            else if (source[i + 1] == '>') { pending = 3; ++i; }
            else pending = 2;
            ++i; continue;
        }
        if (out >= LINE_BYTES - 1) return -1;
        char *word = &storage[out];
        int quote = 0, copied = 0;
        while (i < n) {
            char c = source[i];
            if (quote) {
                if (c == quote) { quote = 0; ++i; continue; }
                if (c == '\\' && source[i + 1] && source[i + 1] == quote) c = source[++i];
                storage[out++] = c; ++i; copied = 1; continue;
            }
            if (c == '\'' || c == '"') { quote = c; ++i; continue; }
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '|' || c == '<' || c == '>') break;
            storage[out++] = c; ++i; copied = 1;
            if (out >= LINE_BYTES - 1) return -1;
        }
        if (quote || !copied) return -1;
        storage[out++] = 0;
        if (pending == 1) {
            if (stages[stage].input) return -1;
            stages[stage].input = word;
        } else if (pending == 2 || pending == 3) {
            if (stages[stage].output) return -1;
            stages[stage].output = word;
            stages[stage].append = pending == 3;
        } else {
            if (stages[stage].argc == ARG_LIMIT) return -1;
            stages[stage].argv[stages[stage].argc++] = word;
        }
        pending = 0;
    }
    if (pending || stages[stage].argc == 0) return -1;
    *stage_count = stage + 1;
    return 0;
}

static int external_stage(const Stage *stage) {
    if (stage->argc == 1) {
        const char *name = stage->argv[0];
        if (name[0] == '/') return 1;
        while (*name) if (*name++ == '/') return 1;
        return 0;
    }
    return same(stage->argv[0], "run") && stage->argc == 2;
}

static void run_cat(const Stage *stage) {
    int input = 0, close_input = 0;
    if (stage->argc > 2) { error_number("cat argument error ", ERR_INVALID); exit(2); }
    if (stage->argc == 2) {
        input = (int)open(stage->argv[1], FD_O_RDONLY);
        if (input < 0) { error_number("cat open error ", input); exit(2); }
        close_input = 1;
    }
    char buffer[IO_BYTES];
    long status = 0;
    unsigned char last = 0;
    int wrote = 0;
    for (;;) {
        const long amount = read(input, buffer, READ_BYTES);
        if (amount < 0) { error_number("cat read error ", amount); status = 1; break; }
        if (amount == 0) break;
        if (user_write_all(1, buffer, (unsigned long)amount) < 0) { error_number("cat write error ", ERR_BROKEN_PIPE); status = 1; break; }
        last = (unsigned char)buffer[amount - 1];
        wrote = 1;
    }
    if (wrote && status == 0 && is_console(1) == 1 && last != '\n') write(1, "\n", 1);
    if (close_input) close(input);
    exit(status);
}

static void child_run(const Stage *stage) {
    if (same(stage->argv[0], "echo")) {
        for (int i = 1; i < stage->argc; ++i) {
            if (i > 1 && user_write_all(1, " ", 1) < 0) exit(1);
            unsigned long n = 0;
            while (stage->argv[i][n]) ++n;
            if (user_write_all(1, stage->argv[i], n) < 0) exit(1);
        }
        if (user_write_all(1, "\n", 1) < 0) exit(1);
        exit(0);
    }
    if (same(stage->argv[0], "cat")) run_cat(stage);
    if (same(stage->argv[0], "pwd") && stage->argc == 1) {
        char cwd[128];
        const long result = getcwd(cwd, sizeof(cwd));
        if (result < 0) { error_number("pwd error ", result); exit(1); }
        unsigned long length = 0;
        while (cwd[length]) ++length;
        if (user_write_all(1, cwd, length) < 0 || user_write_all(1, "\n", 1) < 0) exit(1);
        exit(0);
    }
    if (same(stage->argv[0], "ls") && stage->argc <= 2) {
        char listing[512];
        const long length = fs_list(stage->argc == 2 ? stage->argv[1] : ".", listing, sizeof(listing));
        if (length < 0) { error_number("ls error ", length); exit(1); }
        if (user_write_all(1, listing, (unsigned long)length) < 0) exit(1);
        exit(0);
    }
    if (same(stage->argv[0], "writer") && stage->argc == 1) { exec(PROGRAM_WRITER); error_number("writer exec error ", ERR_INVALID); exit(127); }
    if (same(stage->argv[0], "reader") && stage->argc == 1) { exec(PROGRAM_READER); error_number("reader exec error ", ERR_INVALID); exit(127); }
    if (external_stage(stage)) {
        const char *path = same(stage->argv[0], "run") ? stage->argv[1] : stage->argv[0];
        const long result = exec_path(path);
        error_number("exec error ", result);
        exit(127);
    }
    error_number("command cannot run in a pipeline ", ERR_INVALID);
    exit(127);
}

static void close_all(long pipes[STAGE_LIMIT - 1][2], int files[STAGE_LIMIT][2], int count) {
    for (int i = 0; i < count - 1; ++i) for (int end = 0; end < 2; ++end)
        if (pipes[i][end] >= 0) close(pipes[i][end]);
    for (int i = 0; i < count; ++i) for (int end = 0; end < 2; ++end)
        if (files[i][end] >= 0) close(files[i][end]);
}

static int run_stages(Stage stages[STAGE_LIMIT], int count) {
    long pipes[STAGE_LIMIT - 1][2] = {{-1, -1}, {-1, -1}};
    int files[STAGE_LIMIT][2];
    long pids[STAGE_LIMIT] = {0};
    for (int i = 0; i < STAGE_LIMIT; ++i) files[i][0] = files[i][1] = -1;
    for (int i = 0; i + 1 < count; ++i) {
        if (pipe(pipes[i]) < 0) { error_number("pipe error ", ERR_NO_SLOT); close_all(pipes, files, count); return 1; }
    }
    for (int i = 0; i < count; ++i) {
        if (stages[i].input) {
            files[i][0] = (int)open(stages[i].input, FD_O_RDONLY);
            if (files[i][0] < 0) { error_number("input open error ", files[i][0]); close_all(pipes, files, count); return 1; }
        }
        if (stages[i].output) {
            const long flags = FD_O_WRONLY | FD_O_CREAT | (stages[i].append ? FD_O_APPEND : FD_O_TRUNC);
            files[i][1] = (int)open(stages[i].output, flags);
            if (files[i][1] < 0) { error_number("output open error ", files[i][1]); close_all(pipes, files, count); return 1; }
        }
    }
    int launched = 0;
    for (int i = 0; i < count; ++i) {
        const long child = fork();
        if (child < 0) {
            error_number("fork error ", child);
            close_all(pipes, files, count);
            for (int j = 0; j < launched; ++j) kill(pids[j]);
            for (int j = 0; j < launched; ++j) wait(pids[j], 0);
            return 1;
        }
        if (child == 0) {
            const int input = files[i][0] >= 0 ? files[i][0] : (i > 0 ? (int)pipes[i - 1][0] : 0);
            const int output = files[i][1] >= 0 ? files[i][1] : (i + 1 < count ? (int)pipes[i][1] : 1);
            if (dup2(input, 0) < 0 || dup2(output, 1) < 0) {
                error_number("pipeline setup error ", ERR_BAD_FD); exit(126);
            }
            close_all(pipes, files, count);
            child_run(&stages[i]);
        }
        pids[launched++] = child;
    }
    close_all(pipes, files, count);
    for (int i = 0; i < launched; ++i) {
        long status = 0;
        const long result = wait(pids[i], &status);
        if (result < 0) error_number("wait error ", result);
    }
    return 1;
}

int shell_exec_line(char *line) {
    Stage stages[STAGE_LIMIT];
    int count = 0, has_operator = 0;
    char storage[LINE_BYTES];
    if (parse(line, stages, storage, &count, &has_operator) < 0) {
        for (const char *p = line; *p; ++p) if (*p == '|' || *p == '<' || *p == '>') {
            user_write_all(2, "pipeline syntax error\n", 22);
            return 1;
        }
        return 0;
    }
    const int is_echo = same(stages[0].argv[0], "echo");
    const int is_cat = same(stages[0].argv[0], "cat");
    if (!has_operator && !is_echo && !is_cat) return 0;
    return run_stages(stages, count);
}
