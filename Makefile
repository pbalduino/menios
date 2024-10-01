DOCKER = $(shell which docker)
DOCKER_IMAGE = menios:latest
DOCKER_RUN = run --rm -it --platform linux/amd64 --mount type=bind,source=$$(pwd),target=/mnt

IMAGE_NAME = menios

ARCH ?= x86-64
GCC_DIR = /usr/bin
LIB_DIR = src/libc
CINCLUDE = \
	-I./include \
	-I../../include \
	-I./vendor/acpica/include \
	-I../../vendor/acpica/include

OUTPUT_DIR = ./zig-out/bin
KERNEL_DIR = src/kernel

KERNEL = $(OUTPUT_DIR)/menios

OBJDIR         = obj
LIBDIR         = lib
ACPICA_OBJ = $(OBJDIR)/acpica
KERNEL_OBJ     = $(OBJDIR)/kernel

LIB_ACPICA = $(LIBDIR)/libacpica.a

KERNEL_SRC = $(shell find -L src -type f -name '*.c')
KERNEL_ASM = $(shell find -L src/kernel -type f -name '*.s')
KERNEL_OBJS = $(patsubst %.c, %.o, $(KERNEL_SRC))

ACPICA_SRC = $(shell find -L vendor/acpica -type f -name '*.c')
ACPICA_OBJS := $(patsubst %.c, %.o, $(ACPICA_SRC))

OBJS = $(KERNEL_OBJS) $(ACPICA_OBJS)

override CFLAGS += \
    -Wall \
    -Wextra \
		-Winline \
		-Wfatal-errors \
		-Wno-unused-parameter \
		-static \
    -std=gnu11 \
		-nostdinc \
		-nostdlib \
    -ffreestanding \
    -fno-lto \
    -fno-stack-check \
    -fno-stack-protector \
    -m64 \
    -march=x86-64 \
    -mno-80387 \
    -mno-mmx \
    -mno-red-zone \
    -mno-sse \
    -mno-sse2 \
		-DMENIOS_KERNEL

override CPPFLAGS := \
    $(CINCLUDE) \
    $(CPPFLAGS) \
    -MMD \
    -MP

override LDFLAGS += \
    -static \
		--no-dynamic-linker \
		-L$(LIBDIR) \
		-z noexecstack \
    -T linker.ld \
    -m elf_x86_64 \
    -nostdlib \
    -z max-page-size=0x1000 \
    -z text

override NASMFLAGS += \
    -Wall \
    -f elf64

GCC_KERNEL_OPTS = \
		$(CFLAGS) \
		$(CINCLUDE)

GCC = $(GCC_DIR)/gcc
LD = $(GCC_DIR)/ld
NASM = $(GCC_DIR)/nasm

QEMU_MEMORY = 8
QEMU_X86 = qemu-system-amd64
QEMU_OPTS = -smp cpus=4,cores=2,sockets=2 -hda $(BOOTLOADER) -m $(QEMU_MEMORY) -no-reboot -no-shutdown

OS_NAME = $(shell uname -s | tr A-Z a-z)

.PHONY: clean
all: build

.PHONY: docker
docker:
ifeq ($(OS_NAME),linux)
	@echo Skipping Docker
else
ifeq ($(shell docker images menios -q), )
	docker rmi -f menios && \
	docker build --platform linux/amd64 -t menios:latest .
else
	@echo "The Docker image for meniOS already exists"
endif
endif

.PHONY: build
build: docker
ifeq ($(OS_NAME),linux)
	@set -eux

	zig build

	@echo Building image
	rm -f $(IMAGE_NAME).hdd
	dd if=/dev/zero bs=1M count=0 seek=64 of=$(IMAGE_NAME).hdd
	sgdisk $(IMAGE_NAME).hdd -n 1:2048 -t 1:ef00
	./bin/limine bios-install $(IMAGE_NAME).hdd
	mformat -i $(IMAGE_NAME).hdd@@1M
	mmd -i $(IMAGE_NAME).hdd@@1M ::/EFI ::/EFI/BOOT
	mcopy -i $(IMAGE_NAME).hdd@@1M $(KERNEL) limine.conf ./bin/limine-bios.sys ::/
	mcopy -i $(IMAGE_NAME).hdd@@1M ./bin/BOOTX64.EFI ::/EFI/BOOT

	@echo Building ISO
	# cp /limine/bin/*.bin bin/
	xorriso -as mkisofs -b limine-bios-cd.bin \
        -no-emul-boot -boot-load-size 4 -boot-info-table \
        --efi-boot limine-uefi-cd.bin \
        -efi-boot-part --efi-boot-image --protective-msdos-label \
        ./bin -o $(IMAGE_NAME).iso
	./bin/limine bios-install $(IMAGE_NAME).iso
else
	$(DOCKER) $(DOCKER_RUN) $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make build"
endif

.PHONY: run
run:
	qemu-system-x86_64 -smp cpus=1,maxcpus=1,sockets=1,dies=1,clusters=1,cores=1 -vga std -no-reboot --no-shutdown -M q35 -m size=2G,maxmem=2G -hda menios.hdd -serial file:com1.log -monitor stdio -d int -M hpet=on -usb -machine q35

.PHONY: test
test:
	@set -eux

ifeq ($(OS_NAME),linux)
	@echo "Testing inside Linux"

	for file in $(shell find -L test -type f -name 'test_*.c'); do \
		gcc -I./include $$file test/unity.c src/kernel/mem/kmalloc.c -o "$$file".bin ; \
		echo "Testing $(.c:.bin=$$file)" ; \
		"$$file".bin ; \
		rm "$$file".bin ; \
	done;
else
	@echo "Testing inside Docker"
	$(DOCKER) $(DOCKER_RUN) $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make test"
endif

shell:
	$(DOCKER) $(DOCKER_RUN) $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && /bin/bash"