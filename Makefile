CC := clang
QEMU := qemu-system-riscv64
OBJCOPY := llvm-objcopy
TARGET := build/kernel.elf
KERNEL_SOURCES := main console trap sbi timer page swap vm process scheduler sync workstate syscall elf io splash block gpu desktop input fs path memory
OBJECTS := build/entry.o build/trap_entry.o build/swtch.o build/user_image.o $(addprefix build/kernel_,$(addsuffix .o,$(KERNEL_SOURCES)))
USER_OBJECTS := build/user_entry.o build/user_api.o build/user_print.o build/user_demo.o build/user_sync_demo.o build/user_shell.o build/user_shell_exec.o
APP_OBJECTS := build/user_entry.o build/user_api.o build/user_print.o build/user_app.o
FAN_OBJECTS := build/user_entry.o build/user_api.o build/user_print.o build/user_fan.o
WORKSTATE_OBJECTS := build/user_entry.o build/user_api.o build/user_print.o build/user_workstate_entry.o build/user_workstate.o
ARCH_FLAGS := --target=riscv64-unknown-elf -march=rv64imac_zicsr_zifencei -mabi=lp64 -mcmodel=medany -msmall-data-limit=0 -mno-relax
CFLAGS := $(ARCH_FLAGS) -std=c11 -O2 -g -Wall -Wextra -Werror -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -fno-pie -Iinclude -MMD -MP
LDFLAGS := $(ARCH_FLAGS) -fuse-ld=lld -nostdlib -static -Wl,--build-id=none

.PHONY: all run run-gui run-gui-vnc run-gui-vnc-session disk-image clean
all: $(TARGET)

build:
	mkdir -p build

build/entry.o: kernel/entry.S | build
	$(CC) $(ARCH_FLAGS) -g -c $< -o $@

build/kernel_%.o: kernel/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/trap_entry.o: kernel/trap.S kernel/trap.h | build
	$(CC) $(ARCH_FLAGS) -g -c $< -o $@

build/swtch.o: kernel/swtch.S kernel/context.h | build
	$(CC) $(ARCH_FLAGS) -g -c $< -o $@

build/user_entry.o: user/entry.S | build
	$(CC) $(ARCH_FLAGS) -g -c $< -o $@

build/user_workstate.o: user/workstate.c | build
	$(CC) $(CFLAGS) -Oz -c $< -o $@

build/user_sync_demo.o: user/sync_demo.c | build
	$(CC) $(CFLAGS) -Oz -c $< -o $@

build/user_shell.o: user/shell.c | build
	$(CC) $(CFLAGS) -Oz -c $< -o $@

build/user_shell_exec.o: user/shell_exec.c | build
	$(CC) $(CFLAGS) -Oz -c $< -o $@

build/user_api.o: user/api.c | build
	$(CC) $(CFLAGS) -Oz -c $< -o $@

build/user_%.o: user/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/user.elf: $(USER_OBJECTS) user/linker.ld
	$(CC) $(LDFLAGS) -Wl,-T,user/linker.ld $(USER_OBJECTS) -o $@

build/user.bin: build/user.elf
	$(OBJCOPY) -O binary $< $@

build/app.elf: $(APP_OBJECTS) user/linker.ld
	$(CC) $(LDFLAGS) -Wl,-T,user/linker.ld $(APP_OBJECTS) -o $@

build/app.bin: build/app.elf
	$(OBJCOPY) -O binary $< $@

build/fan.elf: $(FAN_OBJECTS) user/linker.ld
	$(CC) $(LDFLAGS) -Wl,-T,user/linker.ld $(FAN_OBJECTS) -o $@

build/workstate.elf: $(WORKSTATE_OBJECTS) user/linker.ld
	$(CC) $(LDFLAGS) -Wl,-T,user/linker.ld $(WORKSTATE_OBJECTS) -o $@

build/fan.bin: build/fan.elf
	$(OBJCOPY) -O binary $< $@

build/workstate.bin: build/workstate.elf
	$(OBJCOPY) -O binary $< $@

build/user_image.o: kernel/user_image.S build/user.bin build/app.bin build/fan.bin build/workstate.bin | build
	$(CC) $(ARCH_FLAGS) -g -c $< -o $@

$(TARGET): $(OBJECTS) kernel/linker.ld
	$(CC) $(LDFLAGS) -Wl,-T,kernel/linker.ld -Wl,-Map,build/kernel.map $(OBJECTS) -o $@

build/kernel-disk.elf: $(TARGET)
	$(OBJCOPY) --strip-debug $< $@

DISK_IMAGE ?= build/wuwaos.img
SWAP_IMAGE ?= build/wuwaos-swap.img

disk-image: build/kernel-disk.elf tools/make_disk.py tools/make_swap.py | build
	python3 tools/make_disk.py --image $(DISK_IMAGE) --kernel build/kernel-disk.elf
	python3 tools/make_swap.py --image $(SWAP_IMAGE)

run: $(TARGET) disk-image
	$(QEMU) -machine virt -smp 1 -m 128M -nographic -bios default -kernel $(TARGET) -global virtio-mmio.force-legacy=false -drive file=$(DISK_IMAGE),if=none,format=raw,id=wuwaos,cache=writeback -device virtio-blk-device,drive=wuwaos -drive file=$(SWAP_IMAGE),if=none,format=raw,id=swap,cache=writeback -device virtio-blk-device,drive=swap

run-gui: $(TARGET) disk-image
	$(QEMU) -machine virt -smp 1 -m 128M -display $(GUI_DISPLAY) -bios default -kernel $(TARGET) -global virtio-mmio.force-legacy=false -device virtio-gpu-device -device virtio-tablet-device -drive file=$(DISK_IMAGE),if=none,format=raw,id=wuwaos,cache=writeback -device virtio-blk-device,drive=wuwaos -drive file=$(SWAP_IMAGE),if=none,format=raw,id=swap,cache=writeback -device virtio-blk-device,drive=swap

GUI_QEMU_COMMAND = $(QEMU) -name $(GUI_VM_NAME) $(if $(GUI_PIDFILE),-pidfile $(GUI_PIDFILE)) -machine virt -smp 1 -m 128M -display none -bios default -kernel $(TARGET) -global virtio-mmio.force-legacy=false -device virtio-gpu-device -device virtio-tablet-device -vnc 127.0.0.1:$(GUI_VNC_DISPLAY) -qmp tcp:127.0.0.1:$(GUI_QMP_PORT),server=on,wait=off -monitor none -serial $(GUI_SERIAL) -drive file=$(DISK_IMAGE),if=none,format=raw,id=wuwaos,cache=writeback -device virtio-blk-device,drive=wuwaos -drive file=$(SWAP_IMAGE),if=none,format=raw,id=swap,cache=writeback -device virtio-blk-device,drive=swap

run-gui-vnc: $(TARGET) disk-image
	$(GUI_QEMU_COMMAND)

# Launcher entry after it has built the kernel and selected/prepared its images.
run-gui-vnc-session: $(TARGET)
	$(GUI_QEMU_COMMAND)

GUI_DISPLAY ?= gtk
GUI_VNC_DISPLAY ?= 1
GUI_QMP_PORT ?= 4451
GUI_VM_NAME ?= wuwaos
GUI_SERIAL ?= stdio
GUI_PIDFILE ?=

clean:
	rm -f build/*.o build/*.d build/kernel.elf build/kernel-disk.elf build/kernel.map build/user.elf build/user.bin build/app.elf build/app.bin build/fan.elf build/fan.bin build/workstate.elf build/workstate.bin

-include $(wildcard build/*.d)
