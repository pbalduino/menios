GIT_BRANCH = $(shell git branch --show-current)
IMAGE_NAME = menios

DOCKER = $(shell which docker)
DOCKER_IMAGE = $(IMAGE_NAME):$(GIT_BRANCH)
DOCKER_RUN_FLAGS := $(shell if [ -t 1 ]; then printf -- "-it"; fi)

ARCH ?= x86-64
GCC_DIR = /usr/bin
LIB_DIR = src/libc
CINCLUDE = \
	-I./include

BUILD_DIR ?= build

OUTPUT_DIR = $(BUILD_DIR)/bin
KERNEL_DIR = src/kernel

KERNEL = $(OUTPUT_DIR)/kernel.elf

OBJDIR         = $(BUILD_DIR)/obj
LIBDIR         = lib
UACPI_OBJ      = $(OBJDIR)/uacpi
KERNEL_OBJ     = $(OBJDIR)/kernel

KERNEL_SRC = $(shell find -L src -path 'src/usermode' -prune -o -type f -name '*.c' -print)
KERNEL_ASM = $(shell find -L src/kernel -type f \( -name '*.s' -o -name '*.S' \))
KERNEL_OBJS = $(patsubst %.c, %.o, $(KERNEL_SRC))
KERNEL_ASM_OBJS = $(patsubst %.S, %.o, $(filter %.S,$(KERNEL_ASM)))

UACPI_SRC = $(shell find -L vendor/uacpi -type f -name '*.c')
UACPI_OBJS := $(patsubst %.c, %.o, $(UACPI_SRC))

OBJS = $(KERNEL_OBJS) $(KERNEL_ASM_OBJS) $(UACPI_OBJS)
USER_ELF = $(OBJDIR)/usermode/user_demo.elf
USER_ELF_OBJ = $(OBJDIR)/usermode/user_demo_elf.o
USER_ELF_SYMBOL := $(subst .,_,$(subst /,_,$(USER_ELF)))
OBJS += $(USER_ELF_OBJ)

INIT_ELF = $(OBJDIR)/usermode/init.elf
INIT_ELF_OBJ = $(OBJDIR)/usermode/init_elf.o
INIT_ELF_SYMBOL := $(subst .,_,$(subst /,_,$(INIT_ELF)))
OBJS += $(INIT_ELF_OBJ)

-include $(OBJS:.o=.d)

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
		$(CPPFLAGS) \
		$(CFLAGS)

GCC = $(GCC_DIR)/gcc
LD = $(GCC_DIR)/ld
NASM = $(GCC_DIR)/nasm
OBJCOPY = $(GCC_DIR)/objcopy

QEMU_MEMORY = size=2G,maxmem=2G
QEMU_X86_64 = qemu-system-x86_64
QEMU_LOG_FILE=com1.log
QEMU_OPTS = -smp cpus=2,maxcpus=4,sockets=1,dies=1,clusters=1,cores=2 \
	-vga std \
	-no-reboot \
	--no-shutdown \
	-M q35 \
	-m $(QEMU_MEMORY) \
	-device ahci,id=ahci \
	-device ide-hd,drive=hd0,bus=ahci.0 \
	-drive file=$(IMAGE_NAME).hdd,if=none,id=hd0 \
	-usb \
	-device usb-ehci,id=ehci \
	-device usb-mouse \
	-serial file:$(QEMU_LOG_FILE) \
	-monitor stdio \
	-d int \
	-M hpet=on \
	-rtc base=utc,clock=host \
	-device isa-debug-exit,iobase=0xf4,iosize=0x04
# -hda $(IMAGE_NAME).hdd \
# -usb \
#	-device usb-kbd \
# -device usb-ehci,id=ehci \
# -device usb-mouse \
# -device usb-kbd \


define assert_tools
	@for tool in $(1); do \
		if ! command -v $$tool >/dev/null 2>&1; then \
			echo "Missing required tool: $$tool"; \
			exit 1; \
		fi; \
	done
endef

BUILD_REQUIRED_TOOLS := sgdisk mformat mmd mcopy xorriso


LIMINE_PATH ?= $(shell command -v limine 2>/dev/null)
ifeq ($(LIMINE_PATH),)
LIMINE_PATH := $(OUTPUT_DIR)/limine
endif
LIMINE_DIR := $(dir $(LIMINE_PATH))
LIMINE_ASSETS := \
	limine-bios.sys \
	limine-bios-cd.bin \
	limine-uefi-cd.bin \
	limine-bios-pxe.bin \
	limine-bios-hdd.h \
	limine.h
LIMINE_EFI := \
	BOOTX64.EFI \
	BOOTIA32.EFI \
	BOOTAA64.EFI \
	BOOTRISCV64.EFI \
	BOOTLOONGARCH64.EFI


OS_NAME = $(shell uname -s | tr A-Z a-z)

.PHONY: clean
all: build

.PHONY: check
check:
	cppcheck  --force -q --enable=performance,information,missingInclude -I./include -I/Library/Developer/CommandLineTools/usr/lib/clang/14.0.3/include --error-exitcode=3 ./src/kernel

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
	rm -f $(OBJS) $(OBJS:.o=.d)
	mkdir -p $(OUTPUT_DIR)

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
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make build"
endif

%.o: %.S
ifeq ($(OS_NAME),linux)
	$(GCC) $(GCC_KERNEL_OPTS) -c $< -o $@
else
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make build"
endif

$(USER_ELF): src/usermode/user_demo.S linker/user_elf.ld
ifeq ($(OS_NAME),linux)
	@mkdir -p $(OBJDIR)/usermode
	$(GCC) -nostdlib -nostartfiles -ffreestanding -c src/usermode/user_demo.S -o $(OBJDIR)/usermode/user_demo.o
	$(LD) -nostdlib -static -T linker/user_elf.ld -o $@ $(OBJDIR)/usermode/user_demo.o
else
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make $@"
endif

$(USER_ELF_OBJ): $(USER_ELF)
ifeq ($(OS_NAME),linux)
	@mkdir -p $(OBJDIR)/usermode
	$(OBJCOPY) --input binary --output elf64-x86-64 --binary-architecture i386:x86-64 \
		--redefine-sym _binary_$(USER_ELF_SYMBOL)_start=user_demo_elf_start \
		--redefine-sym _binary_$(USER_ELF_SYMBOL)_end=user_demo_elf_end \
		--redefine-sym _binary_$(USER_ELF_SYMBOL)_size=user_demo_elf_size \
		$< $@
else
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make $@"
endif

$(INIT_ELF): src/usermode/init.S linker/user_elf.ld
ifeq ($(OS_NAME),linux)
	@mkdir -p $(OBJDIR)/usermode
	$(GCC) -nostdlib -nostartfiles -ffreestanding -c src/usermode/init.S -o $(OBJDIR)/usermode/init.o
	$(LD) -nostdlib -static -T linker/user_elf.ld -o $@ $(OBJDIR)/usermode/init.o
else
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make $@"
endif

$(INIT_ELF_OBJ): $(INIT_ELF)
ifeq ($(OS_NAME),linux)
	@mkdir -p $(OBJDIR)/usermode
	$(OBJCOPY) --input binary --output elf64-x86-64 --binary-architecture i386:x86-64 \
		--redefine-sym _binary_$(INIT_ELF_SYMBOL)_start=init_elf_start \
		--redefine-sym _binary_$(INIT_ELF_SYMBOL)_end=init_elf_end \
		--redefine-sym _binary_$(INIT_ELF_SYMBOL)_size=init_elf_size \
		$< $@
else
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make $@"
endif

$(OBJDIR)/usermode:
	@mkdir -p $@

$(OBJDIR)/kernel:
	@mkdir -p $@


.PHONY: build
build: docker $(OBJS)
ifeq ($(OS_NAME),linux)
	@set -eux

	if [ ! -x "$(NASM)" ]; then echo "Missing required tool: $(NASM)"; exit 1; fi
	if [ ! -x "$(LD)" ]; then echo "Missing required tool: $(LD)"; exit 1; fi
	$(call assert_tools,$(BUILD_REQUIRED_TOOLS))

	mkdir -p $(OUTPUT_DIR) $(KERNEL_OBJ) $(OBJDIR)/usermode
	rm -rf $(KERNEL_OBJ)/*

	$(NASM) -f elf64 ./src/kernel/lgdt.s
	$(NASM) -f elf64 ./src/kernel/pit.s
	$(NASM) -f elf64 ./src/kernel/lidt.s
	# $(NASM) -f elf64 ./src/kernel/driver/ps2kb/ps2kb_handler.s

	cp ./src/kernel/lgdt.o $(KERNEL_OBJ)
	cp ./src/kernel/pit.o $(KERNEL_OBJ)
	cp ./src/kernel/lidt.o $(KERNEL_OBJ)
	# cp ./src/kernel/driver/ps2kb/ps2kb_handler.o  $(KERNEL_OBJ)

	cp $(OBJS) $(KERNEL_OBJ)

	$(LD) $(LDFLAGS) -o $(KERNEL) $$(find -L $(KERNEL_OBJ) -type f -name '*.o')

	@echo Syncing Limine assets
	@set -eu; \
	limine_path="$(LIMINE_PATH)"; \
	if [ ! -x "$$limine_path" ]; then \
		echo "Limine binary not found at $$limine_path"; \
		exit 1; \
	fi; \
	limine_dir="$(LIMINE_DIR)"; \
	mkdir -p $(OUTPUT_DIR)/EFI/BOOT; \
	if [ "$$limine_path" != "$(OUTPUT_DIR)/limine" ]; then \
		cp "$$limine_path" $(OUTPUT_DIR)/limine; \
	fi; \
	chmod +x $(OUTPUT_DIR)/limine; \
	for file in $(LIMINE_ASSETS); do \
		if [ -f "$$limine_dir/$$file" ]; then \
			cp "$$limine_dir/$$file" $(OUTPUT_DIR)/; \
		fi; \
	done; \
	for file in $(LIMINE_EFI); do \
		if [ -f "$$limine_dir/$$file" ]; then \
			cp "$$limine_dir/$$file" $(OUTPUT_DIR)/EFI/BOOT/; \
		fi; \
	done; \
	for file in limine limine-bios.sys limine-bios-cd.bin limine-uefi-cd.bin BOOTX64.EFI; do \
		if [ ! -f "$(OUTPUT_DIR)/$$file" ] && [ ! -f "$(OUTPUT_DIR)/EFI/BOOT/$$file" ]; then \
			echo "Required Limine asset $$file is missing"; \
			exit 1; \
		fi; \
	done

	@echo Building image
	rm -f $(IMAGE_NAME).hdd
	dd if=/dev/zero bs=1M count=0 seek=64 of=$(IMAGE_NAME).hdd
	sgdisk $(IMAGE_NAME).hdd -n 1:2048:4095 -t 1:ef02
	sgdisk $(IMAGE_NAME).hdd -n 2:4096 -t 2:ef00
	mformat -F -i $(IMAGE_NAME).hdd@@2M
	mmd -i $(IMAGE_NAME).hdd@@2M ::/EFI ::/EFI/BOOT ::/limine ::/boot ::/boot/limine
	mcopy -i $(IMAGE_NAME).hdd@@2M $(KERNEL) limine.conf $(OUTPUT_DIR)/limine-bios.sys ::/
	mcopy -i $(IMAGE_NAME).hdd@@2M $(OUTPUT_DIR)/limine-bios.sys ::/limine/
	mcopy -i $(IMAGE_NAME).hdd@@2M $(OUTPUT_DIR)/limine-bios.sys ::/boot/
	mcopy -i $(IMAGE_NAME).hdd@@2M $(OUTPUT_DIR)/limine-bios.sys ::/boot/limine/
	mcopy -i $(IMAGE_NAME).hdd@@2M $(OUTPUT_DIR)/EFI/BOOT/BOOTX64.EFI ::/EFI/BOOT
	$(OUTPUT_DIR)/limine bios-install $(IMAGE_NAME).hdd 1

	@echo Building ISO
	# cp /limine/bin/*.bin bin/
	xorriso -as mkisofs -b limine-bios-cd.bin \
        -no-emul-boot -boot-load-size 4 -boot-info-table \
        --efi-boot limine-uefi-cd.bin \
        -efi-boot-part --efi-boot-image --protective-msdos-label \
        $(OUTPUT_DIR) -o $(IMAGE_NAME).iso
	$(OUTPUT_DIR)/limine bios-install $(IMAGE_NAME).iso
else
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make build"
endif

.PHONY: run
run:
	$(call assert_tools,$(QEMU_X86_64))
	$(QEMU_X86_64) $(QEMU_OPTS)

.PHONY: test
test: docker
	@set -eux

ifeq ($(OS_NAME),linux)
	@echo "Testing inside Linux"

	# Skip host-unsafe tests until proper stubs land.
	for file in $(shell find -L test -type f -name 'test_*.c' ! -name 'test_kcondvar.c' ! -name 'test_kmalloc.c'); do \
		gcc -std=gnu11 -DMENIOS_NO_DEBUG -DUNITY_EXCLUDE_SETJMP_H -I./include \
			$$file \
			test/unity.c \
			test/stubs.c \
			src/kernel/file.c \
			src/kernel/fs/vfs.c \
			src/kernel/syscall/syscall.c \
			src/kernel/mem/pmm.c \
			src/kernel/console/vprintk.c \
			src/kernel/console/ansi.c \
			src/kernel/proc/kmutex.c \
			src/kernel/timer/tsc.c \
			src/libc/itoa.c \
			src/libc/string.c \
			-o "$$file".bin ; \
		echo "Testing $$file" ; \
		"$$file".bin ; \
		rm "$$file".bin ; \
	done;
else
	@echo "Testing inside Docker"
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make test"
endif

.PHONY: shell
shell:
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && /bin/bash"

.PHONY: build-apps
build-apps:
