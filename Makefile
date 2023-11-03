DOCKER = $(shell which docker)
DOCKER_IMAGE = menios:latest

IMAGE_NAME = menios

ARCH ?= x86-64
GCC_DIR = /usr/bin
LIB_DIR = src/libc
INCLUDE_DIR = \
	./limine

OUTPUT_DIR = bin
KERNEL_DIR = src/kernel

KERNEL = $(OUTPUT_DIR)/kernel.elf

override CFLAGS += \
    -Wall \
    -Wextra \
		-Winline \
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
		-nostdlib \
		-static \
		-c

override CPPFLAGS := \
    -I./limine \
    $(CPPFLAGS) \
    -MMD \
    -MP


override LDFLAGS += \
    -nostdlib \
    -static \
    -pie \
    -z text \
    -z max-page-size=0x1000 \
    -m elf_x86_64 \
		--no-dynamic-linker \
    -T linker.ld

override NASMFLAGS += \
    -Wall \
    -f elf64

GCC_KERNEL_OPTS = \
		$(CFLAGS) \
		-I./include

GCC = $(GCC_DIR)/gcc
LD = $(GCC_DIR)/ld
NASM = $(GCC_DIR)/nasm

QEMU_MEMORY = 8
QEMU_X86 = qemu-system-amd64
QEMU_OPTS = -smp cpus=4,cores=2,sockets=2 -hda $(BOOTLOADER) -m $(QEMU_MEMORY) -no-reboot -no-shutdown

OS_NAME = $(shell uname -s | tr A-Z a-z)

.PHONY: clean
all: build

.PHONY: check
check:
	cppcheck  --force -q --enable=performance,information,missingInclude -I./include -I/Library/Developer/CommandLineTools/usr/lib/clang/14.0.3/include --error-exitcode=3 ./src/kernel

.PHONY: clean
clean:
	rm -rf $(OUTPUT_DIR) || true
	mkdir $(OUTPUT_DIR) || true

.PHONY: docker
docker:
	# docker rmi -f menios && \
	docker build -t menios:latest .

.PHONY: build
build:
	@set +eux

ifeq ($(OS_NAME),linux)
	@echo Building kernel
	$(GCC) $(GCC_KERNEL_OPTS) $(shell find -L . -type f -name '*.c')

	for file in $(shell find -L . -type f -name '*.s'); do \
		$(NASM) -f elf64 $$file ; \
	done;

	$(LD) $(LDFLAGS) -o $(KERNEL) $(shell find -L . -type f -name '*.o')
	@echo Building image
	rm -f $(IMAGE_NAME).hdd
	dd if=/dev/zero bs=1M count=0 seek=64 of=$(IMAGE_NAME).hdd
	sgdisk $(IMAGE_NAME).hdd -n 1:2048 -t 1:ef00
	/limine/bin/limine bios-install $(IMAGE_NAME).hdd
	mformat -i $(IMAGE_NAME).hdd@@1M
	mmd -i $(IMAGE_NAME).hdd@@1M ::/EFI ::/EFI/BOOT
	mcopy -i $(IMAGE_NAME).hdd@@1M $(KERNEL) limine.cfg /limine/bin/limine-bios.sys ::/
	mcopy -i $(IMAGE_NAME).hdd@@1M /limine/bin/BOOTX64.EFI ::/EFI/BOOT

	@echo Building ISO
	cp /limine/bin/*.bin bin/
	xorriso -as mkisofs -b limine-bios-cd.bin \
        -no-emul-boot -boot-load-size 4 -boot-info-table \
        --efi-boot limine-uefi-cd.bin \
        -efi-boot-part --efi-boot-image --protective-msdos-label \
        ./bin -o $(IMAGE_NAME).iso
	/limine/bin/limine bios-install $(IMAGE_NAME).iso
else
	@make docker
	@echo "Building inside Docker"
	$(DOCKER) run -it --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make build"
endif

.PHONY: run
run:
	qemu-system-x86_64 -smp cpus=1,maxcpus=1,sockets=1,dies=1,clusters=1,cores=1 -vga std -no-reboot -M q35 -m size=2G,maxmem=2G -hda menios.hdd -serial file:com1.log -monitor stdio
