CC := clang
QEMU := qemu-system-riscv64
OBJCOPY := llvm-objcopy
TARGET := build/kernel.elf
KERNEL_SOURCES := main console trap vm process syscall
OBJECTS := build/entry.o build/trap_entry.o build/user_image.o $(addprefix build/kernel_,$(addsuffix .o,$(KERNEL_SOURCES)))
USER_OBJECTS := build/user_entry.o build/user_api.o build/user_demo.o
ARCH_FLAGS := --target=riscv64-unknown-elf -march=rv64imac_zicsr_zifencei -mabi=lp64 -mcmodel=medany -msmall-data-limit=0 -mno-relax
CFLAGS := $(ARCH_FLAGS) -std=c11 -O2 -g -Wall -Wextra -Werror -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -fno-pie -Iinclude -MMD -MP
LDFLAGS := $(ARCH_FLAGS) -fuse-ld=lld -nostdlib -static -Wl,--build-id=none

.PHONY: all run clean
all: $(TARGET)

build:
	mkdir -p build

build/entry.o: kernel/entry.S | build
	$(CC) $(ARCH_FLAGS) -g -c $< -o $@

build/kernel_%.o: kernel/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/trap_entry.o: kernel/trap.S kernel/trap.h | build
	$(CC) $(ARCH_FLAGS) -g -c $< -o $@

build/user_entry.o: user/entry.S | build
	$(CC) $(ARCH_FLAGS) -g -c $< -o $@

build/user_%.o: user/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/user.elf: $(USER_OBJECTS) user/linker.ld
	$(CC) $(LDFLAGS) -Wl,-T,user/linker.ld $(USER_OBJECTS) -o $@

build/user.bin: build/user.elf
	$(OBJCOPY) -O binary $< $@

build/user_image.o: kernel/user_image.S build/user.bin | build
	$(CC) $(ARCH_FLAGS) -g -c $< -o $@

$(TARGET): $(OBJECTS) kernel/linker.ld
	$(CC) $(LDFLAGS) -Wl,-T,kernel/linker.ld -Wl,-Map,build/kernel.map $(OBJECTS) -o $@

run: $(TARGET)
	$(QEMU) -machine virt -smp 1 -m 128M -nographic -bios default -kernel $(TARGET)

clean:
	rm -f build/*.o build/*.d build/kernel.elf build/kernel.map build/user.elf build/user.bin

-include $(wildcard build/*.d)
