DOCKER = /usr/local/bin/docker
DOCKER_IMAGE = menios:latest

GCC_DIR = /usr/bin
LIB_DIR = lib
INCLUDE_DIR = include
OUTPUT_DIR = bin
KERNEL_DIR = kernel

KERNEL_SRC = \
	$(KERNEL_DIR)/ata.c \
	$(KERNEL_DIR)/panic.c \
	$(KERNEL_DIR)/pci.c \
	$(KERNEL_DIR)/pmap.c \
	$(KERNEL_DIR)/rtclock.c \
	$(KERNEL_DIR)/storage.c \
	$(OUTPUT_DIR)/mem_page.o

LIB_SRC = $(LIB_DIR)/stdio.c $(LIB_DIR)/stdlib.c $(LIB_DIR)/string.c

BOOT_DIR = boot
BOOT_BIN = $(OUTPUT_DIR)/boot.o
BOOT_SRC = $(BOOT_DIR)/kernel.c $(BOOT_BIN)
BOOT_IMG = $(OUTPUT_DIR)/hda.img
BOOTLOADER= $(OUTPUT_DIR)/boot.bin

SRC = $(BOOT_SRC) $(LIB_SRC) $(KERNEL_SRC)

GCC = $(GCC_DIR)/gcc
GCC_OPTS = -Os -m32 $(SRC) -o $(BOOTLOADER) -nostdlib -ffreestanding -mno-red-zone -fno-exceptions -nostdlib -Wall -Wextra -Werror -T boot/kernel.ld -I $(INCLUDE_DIR)

QEMU_MEMORY = 8
QEMU_X86 = qemu-system-i386
QEMU_OPTS = -hda $(BOOTLOADER) -m $(QEMU_MEMORY) -no-reboot -no-shutdown
# -hdb $(OUTPUT_DIR)/hdd2.img -cdrom $(OUTPUT_DIR)/cdd.img -usb -device usb-mouse

NASM = nasm
NASM_OPTS = -f elf32 $(BOOT_DIR)/boot.s -o $(BOOT_BIN)

OS_NAME = $(shell uname -s | tr A-Z a-z)

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
	$(NASM) -f elf32 $(KERNEL_DIR)/mem_page.s -o $(OUTPUT_DIR)/mem_page.o
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
