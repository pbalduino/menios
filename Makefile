DOCKER = /usr/local/bin/docker
DOCKER_IMAGE = menios:latest

ARCH ?= 386
GCC_DIR = /usr/bin
LIB_DIR = src/libc
BOOT_DIR = src/boot
INCLUDE_DIR = \
	./include \
	./include/libc
OUTPUT_DIR = bin
KERNEL_DIR = src/kernel

KERNEL_SRC = \
	$(KERNEL_DIR)/console.c \
	$(KERNEL_DIR)/init.c \
	$(KERNEL_DIR)/panic.c \
	$(KERNEL_DIR)/printf.c

LIB_SRC = \
	$(LIB_DIR)/stdlib.c \
	$(LIB_DIR)/string.c

BOOT_DIR = src/boot
BOOT_BIN = $(OUTPUT_DIR)/boot.o
BOOT_IMG = $(OUTPUT_DIR)/hda.img
BOOTLOADER= $(OUTPUT_DIR)/boot.bin

BOOT_SRC = \
	$(BOOT_BIN)

SRC = $(BOOT_SRC) $(LIB_SRC) $(KERNEL_SRC)

GCC = $(GCC_DIR)/gcc
GCC_OPTS = -Os -m32 $(SRC) -o $(BOOTLOADER) -nostdlib -ffreestanding -mno-red-zone -fno-exceptions -Wall -Wextra -Werror -T $(BOOT_DIR)/kernel$(ARCH).ld $(foreach dir,$(INCLUDE_DIR),-I $(dir)) -D ARCH=$(ARCH)

QEMU_MEMORY = 8
QEMU_X86 = qemu-system-i386
QEMU_OPTS = -hda $(BOOTLOADER) -m $(QEMU_MEMORY) -no-reboot -no-shutdown
#  -cdrom $(OUTPUT_DIR)/cdd.img -usb -device usb-mouse

NASM = nasm
NASM_OPTS = -f elf32 $(BOOT_DIR)/boot$(ARCH).s -o $(BOOT_BIN)

OS_NAME = $(shell uname -s | tr A-Z a-z)

.PHONY: clean
all: build

.PHONY: clean
clean:
	rm -rf $(OUTPUT_DIR) || true
	mkdir $(OUTPUT_DIR) || true

.PHONY: docker
docker:
	@docker image inspect $(DOCKER_IMAGE) >/dev/null 2>&1 && echo "Using Docker image" || docker build -t menios:latest docker

.PHONY: build
build:
ifeq ($(OS_NAME),linux)
	$(NASM) $(NASM_OPTS)
	$(GCC) $(GCC_OPTS)
else
	@make docker
	@echo "Building inside Docker"
	$(DOCKER) run -it --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make build"
	qemu-img resize $(BOOTLOADER) 8g
endif

.PHONY: run
run:
	$(QEMU_X86) $(QEMU_OPTS)

.PHONY: ftell
fdisk:
	$(GCC) -Wall -Wextra -Werror tools/mbr.c -o bin/mbr && bin/mbr $(BOOTLOADER) fdisk

.PHONY: ftell
ftell:
	$(GCC) -Wall -Wextra -Werror tools/mbr.c -o bin/mbr && bin/mbr $(BOOTLOADER) ftell

.PHONY: debug
debug:
	$(QEMU_X86) $(QEMU_OPTS) -s -S &
	gdb bin/kernel.debug -x gdb.txt
