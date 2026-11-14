#include "syscall_numbers.h"
#include "user/lib/api.h"
#include "user/lib/print.h"

#define CHECK(test) do { if (!(test)) { user_puts("FAIL: exec/fd "); \
    user_number(__LINE__); user_puts("\n"); return 1; } } while (0)

static volatile unsigned char bss[6000];

static int equal(const char *a, const char *b) {
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}

long user_main(long argc, const char *const argv[]) {
    if (argc == 0) return 0;
    CHECK(argc == 3 && argv && !argv[3] && equal(argv[1], "hello") && equal(argv[2], "world"));
    CHECK(bss[5999] == 0);
    bss[5999] = 19;
    CHECK(exec_argv(argv[0], EXEC_MAX_ARGS + 1, argv) == ERR_INVALID);
    CHECK(exec_argv(argv[0], 1, (const char *const *)(uintptr_t)1) == ERR_ADDRESS);
    long child = fork();
    CHECK(child >= 0);
    if (child == 0) return bss[5999];
    long status;
    CHECK(wait(child, &status) == child && status == 19);

    long fd = open("/bin", FD_O_RDONLY);
    CHECK(fd >= 0 && open("/bin", FD_O_RDWR) < 0);
    WuwaFileInfo info;
    CHECK(fstat(fd, &info) == 0 && info.type == WUWA_FILE_DIRECTORY);
    CHECK(fstat(fd, (WuwaFileInfo *)(uintptr_t)1) == ERR_ADDRESS);
    WuwaDirEntry entry;
    CHECK(read(fd, (char *)&entry, sizeof(entry) - 1) == ERR_INVALID);
    CHECK(read(fd, (char *)&entry, sizeof(entry)) == sizeof(entry) && equal(entry.name, "."));
    long alias = dup(fd);
    CHECK(alias >= 0 && read(alias, (char *)&entry, sizeof(entry)) == sizeof(entry) && equal(entry.name, ".."));
    long count;
    int found = 0;
    while ((count = read(fd, (char *)&entry, sizeof(entry))) > 0)
        if (equal(entry.name, "exec-fd.elf")) found = 1;
    CHECK(count == 0 && found && close(alias) == 0 && close(fd) == 0);
    fd = open(argv[0], FD_O_RDONLY);
    CHECK(fd >= 0 && fstat(fd, &info) == 0 && info.type == WUWA_FILE_REGULAR && info.size > 0);
    CHECK(close(fd) == 0 && fstat(fd, &info) == ERR_BAD_FD);
    user_puts("PASS: argv, extended BSS, exec rollback, fstat and directory FD\n");
    return 0;
}
