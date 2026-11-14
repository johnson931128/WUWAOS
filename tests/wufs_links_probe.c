#include "syscall_numbers.h"
#include "user/lib/api.h"
#include "user/lib/print.h"

#define CHECK(test) do { if (!(test)) { user_puts("FAIL: WUFS links "); \
    user_number(__LINE__); user_puts("\n"); return 1; } } while (0)

static unsigned writers_ready;

static long batch_writer(void *argument) {
    long fd = *(long *)argument;
    __atomic_add_fetch(&writers_ready, 1, __ATOMIC_RELEASE);
    while (__atomic_load_n(&writers_ready, __ATOMIC_ACQUIRE) < 2) yield();
    return write(fd, "batch", 5);
}

long user_main(long unused) {
    (void)unused;
    long fd = open("/a", FD_O_CREAT | FD_O_RDWR);
    CHECK(fd >= 0 && write(fd, "x", 1) == 1);
    CHECK(fs_link("/a", "/b") == 0);
    long alias = open("/b", FD_O_RDONLY);
    WuwaFileInfo a, b;
    CHECK(alias >= 0 && fstat(fd, &a) == 0 && fstat(alias, &b) == 0);
    CHECK(a.inode == b.inode && a.links == 2 && b.links == 2);
    CHECK(fs_remove("/a") == 0 && open("/a", FD_O_RDONLY) < 0);
    CHECK(fs_remove("/b") == 0 && open("/b", FD_O_RDONLY) < 0);
    char value = 0;
    CHECK(read(alias, &value, 1) == 1 && value == 'x');
    CHECK(write(fd, "y", 1) == 1 && fstat(alias, &b) == 0 && b.links == 0 && b.size == 2);
    CHECK(close(fd) == 0 && close(alias) == 0 && sleep_ms(100) == 0);
    fd = open("/a", FD_O_CREAT | FD_O_RDWR);
    CHECK(fd >= 0 && fstat(fd, &b) == 0 && b.inode == a.inode && b.size == 0);
    CHECK(close(fd) == 0 && fs_remove("/a") == 0);
    CHECK(mknod("/dev/console", WUWA_DEVICE_CONSOLE) == 0);
    fd = open("/dev/console", FD_O_RDWR);
    CHECK(fd >= 0 && fstat(fd, &a) == 0 && a.type == WUWA_FILE_DEVICE && a.device == WUWA_DEVICE_CONSOLE);
    CHECK(write(fd, "console node OK\n", 16) == 16 && close(fd) == 0);
    long first = open("/batch1", FD_O_CREAT | FD_O_RDWR);
    long second = open("/batch2", FD_O_CREAT | FD_O_RDWR);
    CHECK(first >= 0 && second >= 0);
    long writer = thread_create(batch_writer, &second);
    CHECK(writer > 0 && batch_writer(&first) == 5);
    long status = 0;
    CHECK(thread_join(writer, &status) == writer && status == 5);
    CHECK(close(first) == 0 && close(second) == 0);
    user_puts("PASS: hard link, unlink, last close, console node, batch writes\n");
    return 0;
}
