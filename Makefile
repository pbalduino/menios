BOOT_FOLDER = boot

DOCKER = /usr/local/bin/docker
DOCKER_IMAGE = menios:latest

GCC_FOLDER = /usr/bin
LIB_FOLDER = lib
INCLUDE_FOLDER = include
OUTPUT_FOLDER = bin
KERNEL_FOLDER = kernel

KERNEL_SRC = $(KERNEL_FOLDER)/rtclock.c $(KERNEL_FOLDER)/pmap.c

LIB_SRC = $(LIB_FOLDER)/stdio.c $(LIB_FOLDER)/stdlib.c $(LIB_FOLDER)/string.c

BOOT_BIN = $(OUTPUT_FOLDER)/boot.o
BOOT_SRC = $(BOOT_FOLDER)/kernel.c $(BOOT_BIN)
BOOTLOADER= $(OUTPUT_FOLDER)/boot.bin

SRC = $(BOOT_SRC) $(LIB_SRC) $(KERNEL_SRC)

GCC = $(GCC_FOLDER)/gcc
GCC_OPTS = -Os -m32 $(SRC) -o $(BOOTLOADER) -nostdlib -ffreestanding -mno-red-zone -fno-exceptions -nostdlib -Wall -Wextra -Werror -T boot/kernel.ld -I $(INCLUDE_FOLDER)

QEMU_MEMORY = 8096
QEMU_X86 = qemu-system-i386
QEMU_OPTS = -drive file=$(BOOTLOADER),format=raw,index=1,media=disk -m $(QEMU_MEMORY)

NASM = nasm
NASM_OPTS = -f elf32 $(BOOT_FOLDER)/boot.s -o $(BOOT_BIN)

OS_NAME = $(shell uname -s | tr A-Z a-z)

.PHONY: clean
clean:
	rm -rf $(OUTPUT_FOLDER) || true
	mkdir $(OUTPUT_FOLDER) || true

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
endif

.PHONY: run
run:
	$(QEMU_X86) $(QEMU_OPTS)
