GIT_BRANCH = $(shell git branch --show-current)
IMAGE_NAME = menios

DOCKER = $(shell which docker)
DOCKER_IMAGE = $(IMAGE_NAME):$(GIT_BRANCH)

ARCH ?= x86-64
GCC_DIR = /usr/bin
LIB_DIR = src/libc
CINCLUDE = \
	-I./include

OUTPUT_DIR = bin
KERNEL_DIR = src/kernel

KERNEL = $(OUTPUT_DIR)/kernel.elf

OBJDIR         = obj
LIBDIR         = lib
UACPI_OBJ      = $(OBJDIR)/uacpi
KERNEL_OBJ     = $(OBJDIR)/kernel

KERNEL_SRC = $(shell find -L src -type f -name '*.c')
KERNEL_ASM = $(shell find -L src/kernel -type f -name '*.s')
KERNEL_OBJS = $(patsubst %.c, %.o, $(KERNEL_SRC))

UACPI_SRC = $(shell find -L vendor/uacpi -type f -name '*.c')
UACPI_OBJS := $(patsubst %.c, %.o, $(UACPI_SRC))

OBJS = $(KERNEL_OBJS) $(UACPI_OBJS)

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
		-DMENIOS_KERNEL \
		-DACPI_DEBUG_OUTPUT \
		-DUACPI_KERNEL_INITIALIZATION

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

.PHONY: check
check:
	cppcheck  --force -q --enable=performance,information,missingInclude -I./include -I/Library/Developer/CommandLineTools/usr/lib/clang/14.0.3/include --error-exitcode=3 ./src/kernel

.PHONY: clean
clean:
	rm -rf $(OUTPUT_DIR) || true
	mkdir $(OUTPUT_DIR) || true

.PHONY: docker
docker:
ifeq ($(OS_NAME),linux)
	@echo Skipping Docker
else
ifeq ($(shell docker images $(DOCKER_IMAGE) -q), )
	docker rmi -f $(DOCKER_IMAGE) && \
	docker build --platform linux/amd64 -t $(DOCKER_IMAGE) .
else
	@echo "The Docker image for meniOS already exists"
endif
endif

%.o: %.c
ifeq ($(OS_NAME),linux)
	$(GCC) $(GCC_KERNEL_OPTS) -c $< -o $@
else
	$(DOCKER) run --rm -it --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make build"
endif

.PHONY: build
build: docker $(OBJS)
ifeq ($(OS_NAME),linux)
	@set -eux

	rm -rf $(KERNEL_OBJ)/*

	$(NASM) -f elf64 ./src/kernel/lgdt.s
	$(NASM) -f elf64 ./src/kernel/pit.s
	$(NASM) -f elf64 ./src/kernel/lidt.s

	cp ./src/kernel/lgdt.o $(KERNEL_OBJ)
	cp ./src/kernel/pit.o $(KERNEL_OBJ)
	cp ./src/kernel/lidt.o $(KERNEL_OBJ)

	cp $(OBJS) $(KERNEL_OBJ)

	$(LD) $(LDFLAGS) -o $(KERNEL) $(shell find -L $(KERNEL_OBJ) -type f -name '*.o')

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
	$(DOCKER) run --rm -it --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make build"
endif

.PHONY: run
run:
	qemu-system-x86_64 -smp cpus=1,maxcpus=2,sockets=1,dies=1,clusters=1,cores=2 -vga std -no-reboot --no-shutdown -M q35 -m size=2G,maxmem=2G -hda menios.hdd -serial file:com1.log -monitor stdio -d int -M hpet=on -usb -machine q35 -rtc base=utc,clock=host

.PHONY: test
test: docker
	@set -eux

ifeq ($(OS_NAME),linux)
	@echo "Testing inside Linux"

	for file in $(shell find -L test -type f -name 'test_*.c'); do \
		gcc -DMENIOS_NO_DEBUG -I./include \
			$$file \
			test/unity.c \
			src/kernel/console/vprintk.c \
			src/kernel/mem/kmalloc.c \
			src/libc/itoa.c \
			src/libc/string.c \
			-o "$$file".bin ; \
		echo "Testing $(.c:.bin=$$file)" ; \
		"$$file".bin ; \
		rm "$$file".bin ; \
	done;
else
	@echo "Testing inside Docker"
	$(DOCKER) run --rm -it --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make test"
endif

shell:
	$(DOCKER) run --rm -it --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && /bin/bash"