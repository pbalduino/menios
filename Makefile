DOCKER = $(shell which docker)
DOCKER_IMAGE = menios:latest

IMAGE_NAME = menios

ARCH ?= x86-64
GCC_DIR = /usr/bin
LIB_DIR = src/libc
CINCLUDE = \
	-I./include \
	-I../../include \
	-I./vendor/acpica/include \
	-I../../vendor/acpica/include

OUTPUT_DIR = bin
KERNEL_DIR = src/kernel

KERNEL = $(OUTPUT_DIR)/kernel.elf

OBJDIR         = ./obj
LIBDIR         = lib
LIB_ACPICA_OBJ = $(OBJDIR)/acpica
KERNEL_OBJ     = $(OBJDIR)/kernel

LIB_ACPICA = $(LIBDIR)/libacpica.a

SRCS = $(shell find -L src -type f -name '*.c')
OBJS = $(patsubst src/%.c, $(OBJDIR)/%.o, $(SRCS))

override CFLAGS += \
    -Wall \
    -Wextra \
		-Winline \
		-Wfatal-errors \
		-Wno-unused-parameter \
    -std=gnu11 \
    -ffreestanding \
    -fno-stack-protector \
    -fno-stack-check \
    -fno-lto \
    -m64 \
    -march=x86-64 \
    -mno-80387 \
    -mno-mmx \
    -mno-sse \
    -mno-sse2 \
    -mno-red-zone \
		-nostdlib \
		-nostdinc \
		-static \
		-c

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
ifeq ($(shell docker images menios -q), )
	docker rmi -f menios && \
	docker build --platform linux/amd64 -t menios:latest .
else
	@echo "The Docker image for meniOS already exists"
endif

.PHONY: build_acpica
build_acpica:
	@set -eux

ifneq ($(wildcard $(LIB_ACPICA)),)
	@echo Found $(LIB_ACPICA)
else
ifeq ($(OS_NAME),linux)
	@echo Building $(LIB_ACPICA)

	for file in $(shell find -L vendor/acpica -type f -name '*.c'); do \
		($(GCC) $(GCC_KERNEL_OPTS) $$file) || exit ; \
	done;

	ar rcs $(LIB_ACPICA) $(shell find -L . -type f -name 'ds*.o') $(shell find -L . -type f -name 'ev*.o') $(shell find -L . -type f -name 'ex*.o') $(shell find -L . -type f -name 'hw*.o') $(shell find -L . -type f -name 'ns*.o') $(shell find -L . -type f -name 'ps*.o') $(shell find -L . -type f -name 'tb*.o') $(shell find -L . -type f -name 'ut*.o') $(shell find -L . -type f -name 'menios.o')

	mv *.o $(LIB_ACPICA_OBJ)
	mv *.a $(LIB_ACPICA)

else
	@make docker
	@echo "Building acpica inside Docker"
	$(DOCKER) run --rm -it --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make build_acpica"
endif

endif

.PHONY: build
build: build_acpica
	@set -eux

ifeq ($(OS_NAME),linux)
	@echo Building kernel

	for file in $(shell find -L src -type f -name '*.c'); do \
		($(GCC) $(GCC_KERNEL_OPTS) $$file) || exit ; \
	done;

	for file in $(shell find -L src/kernel -type f -name '*.s'); do \
		$(NASM) -f elf64 $$file ; \
	done;

	mv *.o $(KERNEL_OBJ)
	mv $(KERNEL_DIR)/*.o $(KERNEL_OBJ)
	
	$(LD) $(LDFLAGS) -o $(KERNEL) $(shell find -L obj/kernel -type f -name '*.o') -lacpica 
# $(shell find -L obj/acpica -type f -name '*.o')
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
	@make docker
	@echo "Building inside Docker"
	$(DOCKER) run --rm -it --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make build"
endif

.PHONY: run
run:
	qemu-system-x86_64 -smp cpus=1,maxcpus=1,sockets=1,dies=1,clusters=1,cores=1 -vga std -no-reboot --no-shutdown -M q35 -m size=2G,maxmem=2G -hda menios.hdd -serial file:com1.log -monitor stdio -d int -M hpet=on -usb -machine q35
