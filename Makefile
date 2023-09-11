DOCKER = /usr/local/bin/docker
DOCKER_IMAGE = menios:latest

ARCH ?= x86-64
GCC_DIR = /usr/bin
LIB_DIR = src/libc
INCLUDE_DIR = \
	./limine

OUTPUT_DIR = bin
KERNEL_DIR = src/kernel

override CFILES := $(shell find -L . -type f -name '*.c')

KERNEL = $(OUTPUT_DIR)/kernel.bin

override CFLAGS += \
    -Wall \
    -Wextra \
    -std=gnu11 \
    -ffreestanding \
    -fno-stack-protector \
    -fno-stack-check \
    -fno-lto \
    -fPIE \
    -m64 \
    -march=x86-64 \
    -mno-80387 \
    -mno-mmx \
    -mno-sse \
    -mno-sse2 \
    -mno-red-zone \
		-o out

override CPPFLAGS := \
    -I./limine \
    $(CPPFLAGS) \
    -MMD \
    -MP

override LDFLAGS += \
    -m elf_x86_64 \
    -nostdlib \
    -static \
    -pie \
    --no-dynamic-linker \
    -z text \
    -z max-page-size=0x1000 \
    -T linker.ld

override NASMFLAGS += \
    -Wall \
    -f elf64

GCC_KERNEL_OPTS = \
		$(CFLAGS) \
		-I./limine

GCC = $(GCC_DIR)/gcc

QEMU_MEMORY = 8
QEMU_X86 = qemu-system-amd64
QEMU_OPTS = -smp cpus=4,cores=2,sockets=2 -hda $(BOOTLOADER) -m $(QEMU_MEMORY) -no-reboot -no-shutdown

OS_NAME = $(shell uname -s | tr A-Z a-z)

.PHONY: clean
all: build

.PHONY: check
check:
	cppcheck -q --enable=performance,information,missingInclude -I ./include/libc -I ./include --error-exitcode=3 src

.PHONY: clean
clean:
	rm -rf $(OUTPUT_DIR) || true
	mkdir $(OUTPUT_DIR) || true

.PHONY: docker
docker:
	@docker rmi -f menios && docker build -t menios:latest .

.PHONY: build
build:
	set +eux

ifeq ($(OS_NAME),linux)
	$(GCC) $(GCC_KERNEL_OPTS) $(KERNEL_SRC)
else
	@make docker
	@echo "Building inside Docker"
	$(DOCKER) run -it --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "make build"
	qemu-img resize $(BOOTLOADER) 8g
endif

.PHONY: run
run:
	$(QEMU_X86) $(QEMU_OPTS)
