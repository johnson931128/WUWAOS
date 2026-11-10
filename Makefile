CC := clang
QEMU := qemu-system-riscv64
OBJCOPY := llvm-objcopy
TARGET := build/kernel.elf

KERNEL_SOURCES := \
	kernel/boot/main.c drivers/uart/uart.c kernel/io/console.c \
	arch/riscv/trap.c arch/riscv/sbi.c arch/riscv/timer.c arch/riscv/fdt.c arch/riscv/irq.c \
	kernel/mm/page.c kernel/mm/swap.c kernel/mm/vm.c \
	kernel/proc/process.c kernel/proc/event.c kernel/proc/gate.c kernel/proc/scheduler.c kernel/ipc/sync.c kernel/proc/workstate.c \
	kernel/syscall/syscall.c kernel/exec/elf.c kernel/io/io.c ui/boot/splash.c \
	drivers/virtio/block.c drivers/virtio/gpu.c ui/desktop/desktop.c drivers/virtio/input.c \
	kernel/fs/fs.c kernel/fs/path.c kernel/lib/memory.c kernel/io/tty.c \
	ui/graphics/primitives.c ui/terminal/vt.c ui/terminal/renderer.c ui/input/keymap.c
KERNEL_ASM_SOURCES := arch/riscv/entry.S arch/riscv/trap.S arch/riscv/swtch.S kernel/exec/user_image.S
KERNEL_OBJECTS := $(patsubst %.S,build/obj/%.asm.o,$(KERNEL_ASM_SOURCES)) $(patsubst %.c,build/obj/%.o,$(KERNEL_SOURCES))
BOOT_OBJECTS := build/obj/bootloader/entry.asm.o build/obj/bootloader/boot.o build/obj/drivers/uart/uart.o

USER_COMMON_SOURCES := user/lib/api.c user/lib/print.c
USER_OBJECTS := build/obj/user/lib/entry.asm.o $(patsubst %.c,build/obj/%.o,$(USER_COMMON_SOURCES)) \
	build/obj/user/shell/main.o build/obj/user/shell/sync_demo.o \
	build/obj/user/shell/shell.o build/obj/user/shell/shell_exec.o
APP_OBJECTS := build/obj/user/lib/entry.asm.o $(patsubst %.c,build/obj/%.o,$(USER_COMMON_SOURCES)) build/obj/user/apps/demo/main.o
FAN_OBJECTS := build/obj/user/lib/entry.asm.o $(patsubst %.c,build/obj/%.o,$(USER_COMMON_SOURCES)) build/obj/user/apps/fan/main.o
WORKSTATE_OBJECTS := build/obj/user/lib/entry.asm.o $(patsubst %.c,build/obj/%.o,$(USER_COMMON_SOURCES)) \
	build/obj/user/apps/workstate/main.o build/obj/user/apps/workstate/workstate.o

ARCH_FLAGS := --target=riscv64-unknown-elf -march=rv64imac_zicsr_zifencei -mabi=lp64 -mcmodel=medany -msmall-data-limit=0 -mno-relax
CFLAGS := $(ARCH_FLAGS) -std=c11 -O2 -g -Wall -Wextra -Werror -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -fno-pie -I. -Iinclude -MMD -MP
LDFLAGS := $(ARCH_FLAGS) -fuse-ld=lld -nostdlib -static -Wl,--build-id=none

.PHONY: all run run-disk run-gui run-gui-vnc run-gui-vnc-session disk-image clean
all: $(TARGET)

# Retained local mutex prototype; FS/VM use the upstream KernelGate at runtime.
HOST_CC ?= clang
.PHONY: test-kmutex
test-kmutex:
	mkdir -p build
	$(HOST_CC) -std=c11 -O0 -g -Wall -Wextra -Werror -DWUWAOS_KMUTEX_TEST -I. -Iinclude tests/kmutex_test.c kernel/proc/kmutex.c -o build/kmutex-host-test.exe
	./build/kmutex-host-test.exe

build:
	mkdir -p $@

build/obj:
	mkdir -p $@

build/obj/%.o: %.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

build/obj/%.asm.o: %.S
	mkdir -p $(@D)
	$(CC) $(ARCH_FLAGS) -g -I. -Iinclude -MMD -MP -c $< -o $@

build/obj/user/shell/shell.o: user/shell/shell.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -Oz -c $< -o $@

build/obj/user/shell/shell_exec.o: user/shell/shell_exec.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -Oz -c $< -o $@

build/obj/user/shell/sync_demo.o: user/shell/sync_demo.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -Oz -c $< -o $@

build/obj/user/lib/api.o: user/lib/api.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -Oz -c $< -o $@

build/obj/user/apps/workstate/workstate.o: user/apps/workstate/workstate.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -Oz -c $< -o $@

$(TARGET): $(KERNEL_OBJECTS) arch/riscv/linker.ld
	$(CC) $(LDFLAGS) -Wl,-T,arch/riscv/linker.ld -Wl,-Map,build/kernel.map $(KERNEL_OBJECTS) -o $@

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

build/obj/kernel/exec/user_image.asm.o: build/user.bin build/app.bin build/fan.bin build/workstate.bin

build/kernel-disk.elf: $(TARGET)
	$(OBJCOPY) --strip-all $< $@

build/bootloader.elf: $(BOOT_OBJECTS) bootloader/linker.ld
	$(CC) $(LDFLAGS) -Wl,-T,bootloader/linker.ld $(BOOT_OBJECTS) -o $@

build/boot-flash.img: build/bootloader.elf
	$(OBJCOPY) -O binary $< $@.tmp
	truncate -s 32M $@.tmp
	mv -f $@.tmp $@

DISK_IMAGE ?= build/wuwaos.img
SWAP_IMAGE ?= build/wuwaos-swap.img
BOOT_DISK_IMAGE ?= build/wuwaos-cli.img

disk-image: build/kernel-disk.elf tools/make_disk.py tools/make_swap.py | build
	python3 tools/make_disk.py --image $(DISK_IMAGE) --kernel build/kernel-disk.elf
	python3 tools/make_swap.py --image $(SWAP_IMAGE)

run: $(TARGET) disk-image
	$(QEMU) -machine virt -smp 1 -m 128M -nographic -bios default -kernel $(TARGET) -global virtio-mmio.force-legacy=false -drive file=$(DISK_IMAGE),if=none,format=raw,id=wuwaos,cache=writeback -device virtio-blk-device,drive=wuwaos -drive file=$(SWAP_IMAGE),if=none,format=raw,id=swap,cache=writeback -device virtio-blk-device,drive=swap

run-disk: build/boot-flash.img build/kernel-disk.elf tools/make_disk.py
	python3 tools/make_disk.py --image "$(BOOT_DISK_IMAGE)" --kernel build/kernel-disk.elf
	$(QEMU) -machine virt -smp 1 -m 128M -nographic -bios default \
		-drive if=pflash,unit=0,format=raw,readonly=on,file=build/boot-flash.img \
		-global virtio-mmio.force-legacy=false \
		-drive file="$(BOOT_DISK_IMAGE)",if=none,format=raw,id=wuwaos,cache=writeback \
		-device virtio-blk-device,drive=wuwaos

run-gui: $(TARGET) disk-image
	$(QEMU) -machine virt -smp 1 -m 128M -display $(GUI_DISPLAY) -bios default -kernel $(TARGET) -global virtio-mmio.force-legacy=false -device virtio-gpu-device -device virtio-tablet-device -device virtio-keyboard-device -drive file=$(DISK_IMAGE),if=none,format=raw,id=wuwaos,cache=writeback -device virtio-blk-device,drive=wuwaos -drive file=$(SWAP_IMAGE),if=none,format=raw,id=swap,cache=writeback -device virtio-blk-device,drive=swap

GUI_QEMU_COMMAND = $(QEMU) -name $(GUI_VM_NAME) $(if $(GUI_PIDFILE),-pidfile $(GUI_PIDFILE)) -machine virt -smp 1 -m 128M -display none -bios default -kernel $(TARGET) -global virtio-mmio.force-legacy=false -device virtio-gpu-device -device virtio-tablet-device -device virtio-keyboard-device -vnc 127.0.0.1:$(GUI_VNC_DISPLAY) -qmp tcp:127.0.0.1:$(GUI_QMP_PORT),server=on,wait=off -monitor none -serial $(GUI_SERIAL) -drive file=$(DISK_IMAGE),if=none,format=raw,id=wuwaos,cache=writeback -device virtio-blk-device,drive=wuwaos -drive file=$(SWAP_IMAGE),if=none,format=raw,id=swap,cache=writeback -device virtio-blk-device,drive=swap

run-gui-vnc: $(TARGET) disk-image
	$(GUI_QEMU_COMMAND)

run-gui-vnc-session: $(TARGET)
	$(GUI_QEMU_COMMAND)

GUI_DISPLAY ?= gtk
GUI_VNC_DISPLAY ?= 1
GUI_QMP_PORT ?= 4451
GUI_VM_NAME ?= wuwaos
GUI_SERIAL ?= stdio
GUI_PIDFILE ?=

clean:
	if [ -d build/obj ]; then find build/obj -type f \( -name '*.o' -o -name '*.d' \) -delete; fi
	rm -f build/*.o build/*.d build/kernel.elf build/kernel-disk.elf build/kernel.map build/user.elf build/user.bin build/app.elf build/app.bin build/fan.elf build/fan.bin build/workstate.elf build/workstate.bin
	rm -f build/bootloader.elf build/boot-flash.img build/boot-flash.img.tmp

-include $(patsubst %.o,%.d,$(KERNEL_OBJECTS) $(BOOT_OBJECTS) $(USER_OBJECTS) $(APP_OBJECTS) $(FAN_OBJECTS) $(WORKSTATE_OBJECTS))
