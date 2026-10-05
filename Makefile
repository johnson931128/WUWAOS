CC := clang
QEMU := qemu-system-riscv64
TARGET := build/kernel.elf
OBJECTS := build/entry.o build/main.o
ARCH_FLAGS := --target=riscv64-unknown-elf -march=rv64imac -mabi=lp64 -mcmodel=medany -msmall-data-limit=0
CFLAGS := $(ARCH_FLAGS) -std=c11 -O2 -g -Wall -Wextra -Werror -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -fno-pie
LDFLAGS := $(ARCH_FLAGS) -fuse-ld=lld -nostdlib -static -Wl,-T,kernel/linker.ld -Wl,--build-id=none -Wl,-Map,build/kernel.map

.PHONY: all run clean
all: $(TARGET)

build:
	mkdir -p build

build/entry.o: kernel/entry.S | build
	$(CC) $(ARCH_FLAGS) -g -c $< -o $@

build/main.o: kernel/main.c | build
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS) kernel/linker.ld
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

run: $(TARGET)
	$(QEMU) -machine virt -smp 1 -m 128M -nographic -bios default -kernel $(TARGET)

clean:
	rm -f build/entry.o build/main.o build/kernel.elf build/kernel.map
