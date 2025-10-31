GIT_BRANCH = $(shell git branch --show-current)
IMAGE_NAME = menios

DOCKER = $(shell which docker)
DOCKER_IMAGE = $(IMAGE_NAME):$(GIT_BRANCH)
DOCKER_RUN_FLAGS := $(shell if [ -t 1 ]; then printf -- "-it"; fi)
DOCKER_RUN_FLAGS += --rm
DOCKER_RUN_FLAGS += --platform=linux/amd64
DOCKER_ENV = $(if $(EXTRA_CFLAGS),--env EXTRA_CFLAGS="$(EXTRA_CFLAGS)",)
DOCKER_ENV += $(if $(GCOV_ENABLED),--env GCOV=$(GCOV) --env GCOV_DIR=$(GCOV_DIR),)

OS_NAME = $(shell uname -s | tr A-Z a-z)

EXTRA_CFLAGS ?=
ARCH ?= x86-64
GCC_DIR = /usr/bin
LIB_DIR = src/libc

DEFAULT_CROSS_PREFIX ?= x86_64-elf
CROSS_PREFIX ?= $(DEFAULT_CROSS_PREFIX)

GCOV ?= 0
GCOV_DIR ?= $(BUILD_DIR)/gcov
GCOV_ENABLED := $(filter 1 true TRUE yes YES,$(GCOV))
GCOV_FLAGS := $(if $(GCOV_ENABLED),--coverage,)

ifneq ($(MENIOS_HOST_CC),)
USER_CC := $(MENIOS_HOST_CC)
else ifneq ($(shell command -v $(CROSS_PREFIX)-gcc 2>/dev/null),)
USER_CC := $(CROSS_PREFIX)-gcc
else
USER_CC := gcc
endif

ifneq ($(MENIOS_HOST_AR),)
USER_AR := $(MENIOS_HOST_AR)
else ifneq ($(shell command -v $(CROSS_PREFIX)-ar 2>/dev/null),)
USER_AR := $(CROSS_PREFIX)-ar
else
USER_AR := ar
endif

ifneq ($(MENIOS_HOST_OBJCOPY),)
USER_OBJCOPY := $(MENIOS_HOST_OBJCOPY)
else ifneq ($(shell command -v $(CROSS_PREFIX)-objcopy 2>/dev/null),)
USER_OBJCOPY := $(CROSS_PREFIX)-objcopy
else
USER_OBJCOPY := objcopy
endif

export MENIOS_HOST_CC := $(USER_CC)
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

KERNEL_C_DIRS = \
	src/kernel \
	src/kernel/acpi \
	src/kernel/arch/x86_64 \
	src/kernel/block \
	src/kernel/console \
	src/kernel/core \
	src/kernel/drivers/audio/sb \
	src/kernel/drivers/block/ahci \
	src/kernel/drivers/bus/pciroot \
	src/kernel/drivers/core \
	src/kernel/drivers/input/ps2kb \
	src/kernel/framebuffer \
	src/kernel/fs/core \
	src/kernel/fs/devfs \
	src/kernel/fs/fat32 \
	src/kernel/fs/procfs \
	src/kernel/fs/tmpfs \
	src/kernel/fs/vfs \
	src/kernel/hw \
	src/kernel/input \
	src/kernel/ipc \
	src/kernel/mem \
	src/kernel/proc \
	src/kernel/syscall \
	src/kernel/timer \
	src/kernel/user

KERNEL_SRC = $(sort $(foreach dir,$(KERNEL_C_DIRS),$(wildcard $(dir)/*.c)))

KERNEL_ASM_DIRS = \
	src/kernel \
	src/kernel/arch/x86_64 \
	src/kernel/user

KERNEL_ASM = $(sort $(foreach dir,$(KERNEL_ASM_DIRS),$(wildcard $(dir)/*.s) $(wildcard $(dir)/*.S)))
KERNEL_OBJS = $(patsubst %.c, %.o, $(KERNEL_SRC))
KERNEL_ASM_GAS_OBJS = $(patsubst %.S, %.o, $(filter %.S,$(KERNEL_ASM)))
ARCH_NASM_OBJECTS := lgdt.o pit.o lidt.o

UACPI_SRC = $(shell find -L vendor/uacpi -type f -name '*.c')
UACPI_OBJS := $(patsubst %.c, %.o, $(UACPI_SRC))

OBJS = $(KERNEL_OBJS) $(KERNEL_ASM_GAS_OBJS) $(UACPI_OBJS)
USER_ELF = $(OBJDIR)/usermode/user_demo.elf
USER_ELF_OBJ = $(OBJDIR)/usermode/user_demo_elf.o
USER_ELF_SYMBOL := $(subst .,_,$(subst /,_,$(USER_ELF)))
OBJS += $(USER_ELF_OBJ)

INIT_ELF = $(OBJDIR)/usermode/init.elf
INIT_ELF_OBJ = $(OBJDIR)/usermode/init_elf.o
INIT_ELF_SYMBOL := $(subst .,_,$(subst /,_,$(INIT_ELF)))
OBJS += $(INIT_ELF_OBJ)

KERNEL_OBJ_FILES = $(addprefix $(KERNEL_OBJ)/,$(sort $(OBJS) $(ARCH_NASM_OBJECTS)))

MOSH_ELF = $(OBJDIR)/usermode/mosh.elf
ECHO_ELF = $(OBJDIR)/usermode/echo.elf
CAT_ELF = $(OBJDIR)/usermode/cat.elf
ENV_ELF = $(OBJDIR)/usermode/env.elf
TRUE_ELF = $(OBJDIR)/usermode/true.elf
FALSE_ELF = $(OBJDIR)/usermode/false.elf
LS_ELF = $(OBJDIR)/usermode/ls.elf
KILL_ELF = $(OBJDIR)/usermode/kill.elf
PS_ELF = $(OBJDIR)/usermode/ps.elf
STAT_ELF = $(OBJDIR)/usermode/stat.elf
REALPATH_ELF = $(OBJDIR)/usermode/realpath.elf
MALLOC_STRESS_ELF = $(OBJDIR)/usermode/malloc_stress.elf
MEM_ELF = $(OBJDIR)/usermode/mem.elf
ALARM_DEMO_ELF = $(OBJDIR)/usermode/alarm_demo.elf
TOUCH_ELF = $(OBJDIR)/usermode/touch.elf
SHUTDOWN_ELF = $(OBJDIR)/usermode/shutdown.elf
MKNOD_ELF = $(OBJDIR)/usermode/mknod.elf

USER_PROGRAM_ELFS = $(MOSH_ELF) $(ECHO_ELF) $(CAT_ELF) $(ENV_ELF) $(TRUE_ELF) $(FALSE_ELF) $(LS_ELF) $(KILL_ELF) $(PS_ELF) $(STAT_ELF) $(REALPATH_ELF) $(MALLOC_STRESS_ELF) $(MEM_ELF) $(ALARM_DEMO_ELF) $(TOUCH_ELF) $(MKNOD_ELF) $(SHUTDOWN_ELF)
USERLAND_BINS = mosh echo cat env true false ls kill ps stat realpath malloc_stress mem alarm_demo touch mknod shutdown


BINUTILS_TOOLS = as ld objdump nm ar ranlib readelf objcopy strip strings size addr2line

ARCH_FLAGS := -march=x86-64

CC_VERSION := $(shell $(GCC_DIR)/gcc --version 2>/dev/null | head -1)
ifneq ($(findstring clang,$(CC_VERSION)),)
ARCH_FLAGS := -target x86_64-unknown-elf
endif

USER_CCFLAGS = \
	-nostdlib \
	-nostartfiles \
	-ffreestanding \
	-fno-stack-check \
	-fno-stack-protector \
	-I./include \
	-m64 \
	-mno-80387 \
	-mno-mmx \
	-mno-red-zone \
	-mno-sse \
	-mno-sse2 \
	$(ARCH_FLAGS)

SDK_DIR        = $(BUILD_DIR)/sdk

BINUTILS_SRC_DIR := vendor/binutils-2.45
BINUTILS_BUILD_DIR := $(BUILD_DIR)/binutils-menios
BINUTILS_PREFIX := $(abspath $(SDK_DIR))
BINUTILS_CONFIGURE_FLAGS := --disable-nls --disable-gdb --disable-gprof --disable-libdecnumber --disable-gold
BINUTILS_NATIVE_BUILD_DIR := $(BUILD_DIR)/binutils-menios-native
SDK_INCLUDE_DIR = $(SDK_DIR)/include
SDK_LIB_DIR     = $(SDK_DIR)/lib
SDK_BIN_DIR     = $(SDK_DIR)/bin
SDK_OBJ_DIR     = $(SDK_DIR)/obj

USERLIBC_SOURCES = \
	user/libc/init.c \
	user/libc/stdlib.c \
	user/libc/stdio.c \
	user/libc/input.c \
	user/libc/environ.c \
	user/libc/dirent.c \
	user/libc/realpath.c \
	src/libc/ctype.c \
	src/libc/assert.c \
	src/libc/errno.c \
	src/libc/fcntl.c \
	src/libc/itoa.c \
	src/libc/locale.c \
	src/libc/mman.c \
	src/libc/math.c \
	src/libc/sysv_ipc.c \
	src/libc/string.c \
	src/libc/time.c \
	src/libc/signal.c \
	src/libc/unistd.c \
	src/libc/stat.c

CRT_SOURCES = user/crt/crt0.S

USERLIBC_CFLAGS = $(USER_CCFLAGS) -Iuser/libc

USERLIBC_OBJS = $(patsubst %.c,$(SDK_OBJ_DIR)/%.o,$(USERLIBC_SOURCES))
$(SDK_OBJ_DIR)/src/libc/math.o: USER_CCFLAGS := $(filter-out -mno-80387 -mno-sse -mno-sse2,$(USER_CCFLAGS)) -msse2 -m80387
CRT_OBJS = $(patsubst %.S,$(SDK_OBJ_DIR)/%.o,$(CRT_SOURCES))

SDK_LIB = $(SDK_LIB_DIR)/libmeniosc.a
SDK_STARTUP = $(SDK_LIB_DIR)/crt0.o
SDK_LINKER_SCRIPT = $(SDK_LIB_DIR)/user_elf.ld

ifeq ($(OS_NAME),linux)
USERLAND_DEPS := sdk doom $(USER_ELF) $(USER_PROGRAM_ELFS)
BUILD_DEPS := userland $(OBJS)
else
USERLAND_DEPS :=
BUILD_DEPS :=
endif

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
    -fno-pie \
    -m64 \
    -mno-80387 \
    -mno-mmx \
    -mno-red-zone \
    -mno-sse \
    -mno-sse2 \
    -mcmodel=kernel \
        -DMENIOS_KERNEL \
        -DACPI_DEBUG_OUTPUT \
        -DUACPI_KERNEL_INITIALIZATION
override CFLAGS += $(EXTRA_CFLAGS)
override CFLAGS += $(ARCH_FLAGS)

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
AR = $(GCC_DIR)/ar

QEMU_MEMORY = size=2G,maxmem=2G
QEMU_X86_64 = qemu-system-x86_64
QEMU_LOG_FILE=com1.log
QEMU_OPTS = -smp cpus=2,maxcpus=4,sockets=1,dies=1,clusters=1,cores=2 \
	-vga std \
	-no-reboot \
	-M q35 \
	-m $(QEMU_MEMORY) \
	-device ahci,id=ahci \
	-device ide-hd,drive=hd0,bus=ahci.0 \
	-drive file=$(IMAGE_NAME).hdd,if=none,id=hd0 \
	-usb \
	-device usb-ehci,id=ehci \
	-device usb-mouse \
	-serial file:$(QEMU_LOG_FILE),append=on \
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

.PHONY: console
console: docker
ifeq ($(OS_NAME),linux)
	@echo "Skipping Docker console on Linux host"
else
	$(DOCKER) run -it --rm $(DOCKER_ENV) --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/bash
endif

$(SDK_OBJ_DIR)/%.o: %.c
ifeq ($(OS_NAME),linux)
	@mkdir -p $(dir $@)
	$(USER_CC) $(USERLIBC_CFLAGS) $(EXTRA_CFLAGS) -c $< -o $@
else
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) $(DOCKER_ENV) --platform=linux/amd64 --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make EXTRA_CFLAGS='$(EXTRA_CFLAGS)' $@"
endif

$(SDK_OBJ_DIR)/%.o: %.S
ifeq ($(OS_NAME),linux)
	@mkdir -p $(dir $@)
	$(USER_CC) $(USERLIBC_CFLAGS) $(EXTRA_CFLAGS) -c $< -o $@
else
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) $(DOCKER_ENV) --platform=linux/amd64 --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make EXTRA_CFLAGS='$(EXTRA_CFLAGS)' $@"
endif

$(SDK_LIB): $(USERLIBC_OBJS)
ifeq ($(OS_NAME),linux)
	@mkdir -p $(SDK_LIB_DIR)
	$(USER_AR) rcs $@ $^
else
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) $(DOCKER_ENV) --platform=linux/amd64 --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make EXTRA_CFLAGS='$(EXTRA_CFLAGS)' $@"
endif

$(SDK_LIB_DIR)/crt0.o: $(SDK_OBJ_DIR)/user/crt/crt0.o
ifeq ($(OS_NAME),linux)
	@mkdir -p $(SDK_LIB_DIR)
	cp $< $@
else
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) $(DOCKER_ENV) --platform=linux/amd64 --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make EXTRA_CFLAGS='$(EXTRA_CFLAGS)' $@"
endif

$(SDK_LINKER_SCRIPT): linker/user_elf.ld
ifeq ($(OS_NAME),linux)
	@mkdir -p $(SDK_LIB_DIR)
	cp $< $@
else
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) $(DOCKER_ENV) --platform=linux/amd64 --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make EXTRA_CFLAGS='$(EXTRA_CFLAGS)' $@"
endif

$(SDK_DIR)/.headers-stamp: $(shell find include -type f)
ifeq ($(OS_NAME),linux)
	@rm -rf $(SDK_INCLUDE_DIR)
	@mkdir -p $(SDK_INCLUDE_DIR)
	cp -R include/. $(SDK_INCLUDE_DIR)/
	@touch $@
else
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) $(DOCKER_ENV) --platform=linux/amd64 --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make EXTRA_CFLAGS='$(EXTRA_CFLAGS)' $@"
endif

$(SDK_DIR)/.tools-stamp: $(SDK_LIB) $(SDK_STARTUP) tools/menios-gcc.sh tools/menios-ar.sh tools/menios-ranlib.sh
ifeq ($(OS_NAME),linux)
	@mkdir -p $(SDK_BIN_DIR)
	cp tools/menios-gcc.sh $(SDK_BIN_DIR)/menios-gcc
	chmod +x $(SDK_BIN_DIR)/menios-gcc
	cp tools/menios-ar.sh $(SDK_BIN_DIR)/menios-ar
	cp tools/menios-ar.sh $(SDK_BIN_DIR)/x86_64-menios-ar
	chmod +x $(SDK_BIN_DIR)/menios-ar $(SDK_BIN_DIR)/x86_64-menios-ar
	cp tools/menios-ranlib.sh $(SDK_BIN_DIR)/menios-ranlib
	cp tools/menios-ranlib.sh $(SDK_BIN_DIR)/x86_64-menios-ranlib
	chmod +x $(SDK_BIN_DIR)/menios-ranlib $(SDK_BIN_DIR)/x86_64-menios-ranlib
	@touch $@
else
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) $(DOCKER_ENV) --platform=linux/amd64 --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make EXTRA_CFLAGS='$(EXTRA_CFLAGS)' $@"
endif

.PHONY: sdk
sdk: docker $(SDK_LIB) $(SDK_STARTUP) $(SDK_LINKER_SCRIPT) $(SDK_DIR)/.headers-stamp $(SDK_DIR)/.tools-stamp

%.o: %.c
ifeq ($(OS_NAME),linux)
	$(GCC) $(GCC_KERNEL_OPTS) -c $< -o $@
else
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) $(DOCKER_ENV) --platform=linux/amd64 --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make build"
endif

%.o: %.S
ifeq ($(OS_NAME),linux)
	$(GCC) $(GCC_KERNEL_OPTS) -c $< -o $@
else
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) $(DOCKER_ENV) --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make build"
endif

$(USER_ELF): src/usermode/user_demo.S linker/user_elf.ld
ifeq ($(OS_NAME),linux)
	@mkdir -p $(OBJDIR)/usermode
	$(GCC) -nostdlib -nostartfiles -ffreestanding -c src/usermode/user_demo.S -o $(OBJDIR)/usermode/user_demo.o
	$(LD) -nostdlib -static -T linker/user_elf.ld -o $@ $(OBJDIR)/usermode/user_demo.o
else
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) $(DOCKER_ENV) --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make EXTRA_CFLAGS='$(EXTRA_CFLAGS)' $@"
endif

$(USER_ELF_OBJ): $(USER_ELF)
ifeq ($(OS_NAME),linux)
	@mkdir -p $(OBJDIR)/usermode
	$(USER_OBJCOPY) --input binary --output elf64-x86-64 --binary-architecture i386:x86-64 \
		--redefine-sym _binary_$(USER_ELF_SYMBOL)_start=user_demo_elf_start \
		--redefine-sym _binary_$(USER_ELF_SYMBOL)_end=user_demo_elf_end \
		--redefine-sym _binary_$(USER_ELF_SYMBOL)_size=user_demo_elf_size \
		$< $@
else
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) $(DOCKER_ENV) --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make EXTRA_CFLAGS='$(EXTRA_CFLAGS)' $@"
endif

$(INIT_ELF): src/usermode/init.c linker/user_elf.ld
ifeq ($(OS_NAME),linux)
	@mkdir -p $(OBJDIR)/usermode
	$(GCC) $(USER_CCFLAGS) $(EXTRA_CFLAGS) -c src/usermode/init.c -o $(OBJDIR)/usermode/init.o
	$(LD) -nostdlib -static -T linker/user_elf.ld -o $@ $(OBJDIR)/usermode/init.o
else
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) $(DOCKER_ENV) --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make EXTRA_CFLAGS='$(EXTRA_CFLAGS)' $@"
endif

$(INIT_ELF_OBJ): $(INIT_ELF)
ifeq ($(OS_NAME),linux)
	@mkdir -p $(OBJDIR)/usermode
	$(USER_OBJCOPY) --input binary --output elf64-x86-64 --binary-architecture i386:x86-64 \
		--redefine-sym _binary_$(INIT_ELF_SYMBOL)_start=init_elf_start \
		--redefine-sym _binary_$(INIT_ELF_SYMBOL)_end=init_elf_end \
		--redefine-sym _binary_$(INIT_ELF_SYMBOL)_size=init_elf_size \
		$< $@
else
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) $(DOCKER_ENV) --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make EXTRA_CFLAGS='$(EXTRA_CFLAGS)' $@"
endif

$(MOSH_ELF): app/mosh/mosh.c | sdk
ifeq ($(OS_NAME),linux)
	@mkdir -p $(dir $@)
	$(SDK_BIN_DIR)/menios-gcc $(EXTRA_CFLAGS) $< -o $@
else
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) $(DOCKER_ENV) --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make EXTRA_CFLAGS='$(EXTRA_CFLAGS)' $@"
endif

$(ECHO_ELF): app/echo/echo.c | sdk
ifeq ($(OS_NAME),linux)
	@mkdir -p $(dir $@)
	$(SDK_BIN_DIR)/menios-gcc $(EXTRA_CFLAGS) $< -o $@
else
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) $(DOCKER_ENV) --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make EXTRA_CFLAGS='$(EXTRA_CFLAGS)' $@"
endif

$(CAT_ELF): app/cat/cat.c | sdk
ifeq ($(OS_NAME),linux)
	@mkdir -p $(dir $@)
	$(SDK_BIN_DIR)/menios-gcc $(EXTRA_CFLAGS) $< -o $@
else
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) $(DOCKER_ENV) --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make EXTRA_CFLAGS='$(EXTRA_CFLAGS)' $@"
endif

$(ENV_ELF): app/env/env.c | sdk
ifeq ($(OS_NAME),linux)
	@mkdir -p $(dir $@)
	$(SDK_BIN_DIR)/menios-gcc $(EXTRA_CFLAGS) $< -o $@
else
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) $(DOCKER_ENV) --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make EXTRA_CFLAGS='$(EXTRA_CFLAGS)' $@"
endif

$(TRUE_ELF): app/true/true.c | sdk
ifeq ($(OS_NAME),linux)
	@mkdir -p $(dir $@)
	$(SDK_BIN_DIR)/menios-gcc $(EXTRA_CFLAGS) $< -o $@
else
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) $(DOCKER_ENV) --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make EXTRA_CFLAGS='$(EXTRA_CFLAGS)' $@"
endif

$(FALSE_ELF): app/false/false.c | sdk
ifeq ($(OS_NAME),linux)
	@mkdir -p $(dir $@)
	$(SDK_BIN_DIR)/menios-gcc $(EXTRA_CFLAGS) $< -o $@
else
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) $(DOCKER_ENV) --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make EXTRA_CFLAGS='$(EXTRA_CFLAGS)' $@"
endif

$(LS_ELF): app/ls/ls.c | sdk
ifeq ($(OS_NAME),linux)
	@mkdir -p $(dir $@)
	$(SDK_BIN_DIR)/menios-gcc $(EXTRA_CFLAGS) $< -o $@
else
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) $(DOCKER_ENV) --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make EXTRA_CFLAGS='$(EXTRA_CFLAGS)' $@"
endif

$(STAT_ELF): app/stat/stat.c | sdk
ifeq ($(OS_NAME),linux)
	@mkdir -p $(dir $@)
	$(SDK_BIN_DIR)/menios-gcc $(EXTRA_CFLAGS) $< -o $@
else
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) $(DOCKER_ENV) --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make EXTRA_CFLAGS='$(EXTRA_CFLAGS)' $@"
endif

$(REALPATH_ELF): app/realpath/realpath.c | sdk
ifeq ($(OS_NAME),linux)
	@mkdir -p $(dir $@)
	$(SDK_BIN_DIR)/menios-gcc $(EXTRA_CFLAGS) $< -o $@
else
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) $(DOCKER_ENV) --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make EXTRA_CFLAGS='$(EXTRA_CFLAGS)' $@"
endif

$(PS_ELF): app/ps/ps.c | sdk
ifeq ($(OS_NAME),linux)
	@mkdir -p $(dir $@)
	$(SDK_BIN_DIR)/menios-gcc $(EXTRA_CFLAGS) $< -o $@
else
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) $(DOCKER_ENV) --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make EXTRA_CFLAGS='$(EXTRA_CFLAGS)' $@"
endif

$(MEM_ELF): app/mem/mem.c | sdk
ifeq ($(OS_NAME),linux)
	@mkdir -p $(dir $@)
	$(SDK_BIN_DIR)/menios-gcc $(EXTRA_CFLAGS) $< -o $@
else
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) $(DOCKER_ENV) --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make EXTRA_CFLAGS='$(EXTRA_CFLAGS)' $@"
endif

$(ALARM_DEMO_ELF): app/alarm_demo/alarm_demo.c | sdk
ifeq ($(OS_NAME),linux)
	@mkdir -p $(dir $@)
	$(SDK_BIN_DIR)/menios-gcc $(EXTRA_CFLAGS) $< -o $@
else
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) $(DOCKER_ENV) --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make EXTRA_CFLAGS='$(EXTRA_CFLAGS)' $@"
endif

$(TOUCH_ELF): app/touch/touch.c | sdk
ifeq ($(OS_NAME),linux)
	@mkdir -p $(dir $@)
	$(SDK_BIN_DIR)/menios-gcc $(EXTRA_CFLAGS) $< -o $@
else
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) $(DOCKER_ENV) --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make EXTRA_CFLAGS='$(EXTRA_CFLAGS)' $@"
endif

$(MKNOD_ELF): app/mknod/mknod.c | sdk
ifeq ($(OS_NAME),linux)
	@mkdir -p $(dir $@)
	$(SDK_BIN_DIR)/menios-gcc $(EXTRA_CFLAGS) $< -o $@
else
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) $(DOCKER_ENV) --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make EXTRA_CFLAGS='$(EXTRA_CFLAGS)' $@"
endif

$(SHUTDOWN_ELF): app/shutdown/shutdown.c | sdk
ifeq ($(OS_NAME),linux)
	@mkdir -p $(dir $@)
	$(SDK_BIN_DIR)/menios-gcc $(EXTRA_CFLAGS) $< -o $@
else
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) $(DOCKER_ENV) --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make EXTRA_CFLAGS='$(EXTRA_CFLAGS)' $@"
endif

$(MALLOC_STRESS_ELF): app/malloc_stress/malloc_stress.c | sdk
ifeq ($(OS_NAME),linux)
	@mkdir -p $(dir $@)
	$(SDK_BIN_DIR)/menios-gcc $(EXTRA_CFLAGS) $< -o $@
else
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) $(DOCKER_ENV) --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make EXTRA_CFLAGS='$(EXTRA_CFLAGS)' $@"
endif

$(KILL_ELF): app/kill/kill.c | sdk
ifeq ($(OS_NAME),linux)
	@mkdir -p $(dir $@)
	$(SDK_BIN_DIR)/menios-gcc $(EXTRA_CFLAGS) $< -o $@
else
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) $(DOCKER_ENV) --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make EXTRA_CFLAGS='$(EXTRA_CFLAGS)' $@"
endif

$(OBJDIR)/usermode:
	@mkdir -p $@

$(OBJDIR)/kernel:
	@mkdir -p $@


.PHONY: userland
userland: $(USERLAND_DEPS) doom
ifeq ($(OS_NAME),linux)
	@$(MAKE) sdk $(USER_ELF) $(USER_PROGRAM_ELFS)
	@rm -rf $(OUTPUT_DIR)/bin
	@mkdir -p $(OUTPUT_DIR)/bin
	cp $(USER_ELF) $(OUTPUT_DIR)/bin/user_demo
	@for prog in $(USERLAND_BINS); do \
		cp $(OBJDIR)/usermode/$$prog.elf $(OUTPUT_DIR)/bin/$$prog; \
	done
	@if [ -f "$(OUTPUT_DIR)/doom.elf" ]; then \
		echo "[DOOM] Installing doom.elf into $(OUTPUT_DIR)/bin"; \
		cp $(OUTPUT_DIR)/doom.elf $(OUTPUT_DIR)/bin/doom; \
	else \
		echo "[DOOM] doom.elf not linked (libc gaps), skipping binary install"; \
	fi
else
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) $(DOCKER_ENV) --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make EXTRA_CFLAGS='$(EXTRA_CFLAGS)' userland"
endif

.PHONY: build
build: $(BUILD_DEPS)
ifeq ($(OS_NAME),linux)
	@set -eux

	if [ ! -x "$(NASM)" ]; then echo "Missing required tool: $(NASM)"; exit 1; fi
	if [ ! -x "$(LD)" ]; then echo "Missing required tool: $(LD)"; exit 1; fi
	$(call assert_tools,$(BUILD_REQUIRED_TOOLS))

	mkdir -p $(OUTPUT_DIR) $(KERNEL_OBJ) $(OBJDIR)/usermode
	find $(KERNEL_OBJ) -type f -delete

	$(NASM) -f elf64 ./src/kernel/lgdt.s
	$(NASM) -f elf64 ./src/kernel/pit.s
	$(NASM) -f elf64 ./src/kernel/lidt.s

	cp ./src/kernel/lgdt.o $(KERNEL_OBJ)
	cp ./src/kernel/pit.o $(KERNEL_OBJ)
	cp ./src/kernel/lidt.o $(KERNEL_OBJ)

	$(MAKE) $(OBJS)

	@for obj in $(OBJS); do \
		dest_dir="$(KERNEL_OBJ)/$$(dirname "$$obj")"; \
		mkdir -p "$$dest_dir"; \
		cp -f "$$obj" "$$dest_dir/"; \
	done
	$(LD) $(LDFLAGS) -o $(KERNEL) $(KERNEL_OBJ_FILES)

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

	@# Stage toolchain binaries for installer image when available
	@for tool in $(BINUTILS_TOOLS); do \
		if [ -f "$(SDK_BIN_DIR)/$$tool" ]; then \
			cp $(SDK_BIN_DIR)/$$tool $(OUTPUT_DIR)/bin/$$tool; \
		fi; \
	done

	@echo Building image
	rm -f $(IMAGE_NAME).hdd
	dd if=/dev/zero bs=1M count=0 seek=128 of=$(IMAGE_NAME).hdd
	sgdisk $(IMAGE_NAME).hdd -n 1:2048:4095 -t 1:ef02
	sgdisk $(IMAGE_NAME).hdd -n 2:4096 -t 2:ef00
	mformat -F -i $(IMAGE_NAME).hdd@@2M
	mmd -i $(IMAGE_NAME).hdd@@2M ::/EFI ::/EFI/BOOT ::/limine ::/boot ::/boot/limine > /dev/null 2>&1 || true
	mmd -i $(IMAGE_NAME).hdd@@2M ::/bin > /dev/null 2>&1 || true
	mmd -i $(IMAGE_NAME).hdd@@2M ::/doom > /dev/null 2>&1 || true
	mmd -i $(IMAGE_NAME).hdd@@2M ::/home > /dev/null 2>&1 || true
	mkdir -p $(OUTPUT_DIR)/home
	printf 'echo Welcome to meniOS 0.1.666\n' > $(OUTPUT_DIR)/home/.moshrc
	cp samples/hello.s $(OUTPUT_DIR)/home/hello.s
	mcopy -i $(IMAGE_NAME).hdd@@2M $(KERNEL) limine.conf $(OUTPUT_DIR)/limine-bios.sys ::/
	mcopy -i $(IMAGE_NAME).hdd@@2M $(OUTPUT_DIR)/limine-bios.sys ::/limine/
	mcopy -i $(IMAGE_NAME).hdd@@2M $(OUTPUT_DIR)/limine-bios.sys ::/boot/
	mcopy -i $(IMAGE_NAME).hdd@@2M $(OUTPUT_DIR)/limine-bios.sys ::/boot/limine/
	mcopy -i $(IMAGE_NAME).hdd@@2M $(OUTPUT_DIR)/EFI/BOOT/BOOTX64.EFI ::/EFI/BOOT
	mcopy -o -i $(IMAGE_NAME).hdd@@2M $(OUTPUT_DIR)/bin/user_demo ::/bin/user_demo
	mcopy -o -i $(IMAGE_NAME).hdd@@2M $(OUTPUT_DIR)/bin/mosh ::/bin/mosh
	mcopy -o -i $(IMAGE_NAME).hdd@@2M $(OUTPUT_DIR)/bin/echo ::/bin/echo
	mcopy -o -i $(IMAGE_NAME).hdd@@2M $(OUTPUT_DIR)/bin/cat ::/bin/cat
	mcopy -o -i $(IMAGE_NAME).hdd@@2M $(OUTPUT_DIR)/bin/env ::/bin/env
	mcopy -o -i $(IMAGE_NAME).hdd@@2M $(OUTPUT_DIR)/bin/true ::/bin/true
	mcopy -o -i $(IMAGE_NAME).hdd@@2M $(OUTPUT_DIR)/bin/false ::/bin/false
	mcopy -o -i $(IMAGE_NAME).hdd@@2M $(OUTPUT_DIR)/bin/ls ::/bin/ls
	mcopy -o -i $(IMAGE_NAME).hdd@@2M $(OUTPUT_DIR)/bin/kill ::/bin/kill
	mcopy -o -i $(IMAGE_NAME).hdd@@2M $(OUTPUT_DIR)/bin/ps ::/bin/ps
	mcopy -o -i $(IMAGE_NAME).hdd@@2M $(OUTPUT_DIR)/bin/stat ::/bin/stat
	mcopy -o -i $(IMAGE_NAME).hdd@@2M $(OUTPUT_DIR)/bin/realpath ::/bin/realpath
	mcopy -o -i $(IMAGE_NAME).hdd@@2M $(OUTPUT_DIR)/bin/readelf ::/bin/readelf
	mcopy -o -i $(IMAGE_NAME).hdd@@2M $(OUTPUT_DIR)/bin/malloc_stress ::/bin/malloc_stress
	mcopy -o -i $(IMAGE_NAME).hdd@@2M $(OUTPUT_DIR)/bin/mem ::/bin/mem
	mcopy -o -i $(IMAGE_NAME).hdd@@2M $(OUTPUT_DIR)/bin/alarm_demo ::/bin/alarm_demo
	mcopy -o -i $(IMAGE_NAME).hdd@@2M $(OUTPUT_DIR)/bin/touch ::/bin/touch
	mcopy -o -i $(IMAGE_NAME).hdd@@2M $(OUTPUT_DIR)/bin/shutdown ::/bin/shutdown
	for tool in $(BINUTILS_TOOLS); do \
	if [ -f "$(OUTPUT_DIR)/bin/$$tool" ]; then \
		mcopy -o -i $(IMAGE_NAME).hdd@@2M $(OUTPUT_DIR)/bin/$$tool ::/bin/$$tool; \
	fi; \
	done
	mcopy -i $(IMAGE_NAME).hdd@@2M $(OUTPUT_DIR)/home/.moshrc ::/home/.moshrc
	mcopy -i $(IMAGE_NAME).hdd@@2M $(OUTPUT_DIR)/home/hello.s ::/home/hello.s
	if [ -f "$(OUTPUT_DIR)/bin/doom" ]; then \
		mcopy -i $(IMAGE_NAME).hdd@@2M $(OUTPUT_DIR)/bin/doom ::/bin/doom; \
	else \
		echo "[DOOM] Skipping doom binary copy (binary not linked)"; \
	fi
	if [ -f "$(OUTPUT_DIR)/doom.wad" ]; then \
		mcopy -i $(IMAGE_NAME).hdd@@2M $(OUTPUT_DIR)/doom.wad ::/doom/doom.wad; \
	else \
		echo "[DOOM] No doom.wad found in $(OUTPUT_DIR) (optional)"; \
	fi
	if [ -f "$(OUTPUT_DIR)/doom2.wad" ]; then \
		mcopy -i $(IMAGE_NAME).hdd@@2M $(OUTPUT_DIR)/doom2.wad ::/doom/doom2.wad; \
	else \
		echo "[DOOM] No doom2.wad found in $(OUTPUT_DIR) (optional)"; \
	fi
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
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) $(DOCKER_ENV) --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make EXTRA_CFLAGS='$(EXTRA_CFLAGS)' build"
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
ifneq ($(GCOV_ENABLED),)
	@echo "Coverage instrumentation enabled; cleaning previous profiling data"
	@rm -rf $(GCOV_DIR)
	@mkdir -p $(GCOV_DIR)
	@find . -name '*.gcda' -delete || true
	@find . -name '*.gcov' -delete || true
endif

	# Skip host-unsafe tests until proper stubs land.
	for file in $(shell find -L test -type f -name 'test_*.c' ! -name 'test_kmalloc.c' ! -name 'test_malloc_stress.c' ! -name 'test_buddy_allocator.c' ! -name 'test_malloc_stats.c' ! -name 'test_malloc_direct.c' ! -name 'test_heap_virtual.c' ! -name 'test_scanf.c' ! -name 'test_system.c'); do \
		gcc $(GCOV_FLAGS) -std=gnu11 -DMENIOS_NO_DEBUG -DMENIOS_HOST_TEST -DUNITY_EXCLUDE_SETJMP_H -I./include \
			$$file \
			test/unity.c \
			test/stubs.c \
			src/kernel/file.c \
		src/kernel/fs/vfs/vfs.c \
		src/kernel/fs/core/pipe.c \
		src/kernel/fs/tmpfs/tmpfs.c \
		src/kernel/fs/procfs/procfs.c \
		src/kernel/fs/devfs/devfs.c \
		src/kernel/syscall/syscall.c \
		src/kernel/syscall/entry.c \
			src/kernel/mem/pmm.c \
			src/kernel/console/vprintk.c \
		src/kernel/console/ansi.c \
		src/kernel/proc/kcondvar.c \
		src/kernel/proc/kmutex.c \
	src/kernel/proc/signal.c \
	src/kernel/ipc/shm.c \
			src/kernel/user/vm_region.c \
		src/kernel/timer/tsc.c \
			src/kernel/block/block_cache.c \
		test/stubs_framebuffer.c \
		test/stubs_fat32.c \
		src/libc/itoa.c \
		src/libc/string.c \
		src/libc/time.c \
		src/libc/errno.c \
		-o "$$file".bin ; \
		echo "Testing $$file" ; \
		"$$file".bin ; \
		rc=$$?; \
		rm "$$file".bin ; \
		if [ $$rc -ne 0 ]; then exit $$rc; fi; \
	done;

	# Host-stubbed allocator stress test exercises user/libc/stdlib.c explicitly.
	gcc $(GCOV_FLAGS) -std=gnu11 -DMENIOS_NO_DEBUG -DMENIOS_HOST_TEST -DUNITY_EXCLUDE_SETJMP_H -I./include \
		test/test_buddy_allocator.c \
		test/unity.c \
		test/stubs.c \
		src/kernel/file.c \
		src/kernel/fs/vfs/vfs.c \
		src/kernel/fs/core/pipe.c \
		src/kernel/fs/tmpfs/tmpfs.c \
		src/kernel/fs/devfs/devfs.c \
		src/kernel/syscall/syscall.c \
			src/kernel/syscall/entry.c \
		src/kernel/mem/pmm.c \
		src/kernel/console/vprintk.c \
		src/kernel/console/ansi.c \
		src/kernel/proc/kcondvar.c \
		src/kernel/proc/kmutex.c \
		src/kernel/proc/signal.c \
		src/kernel/ipc/shm.c \
		src/kernel/user/vm_region.c \
		src/kernel/timer/tsc.c \
		src/kernel/block/block_cache.c \
		test/stubs_framebuffer.c \
		test/stubs_fat32.c \
		src/libc/itoa.c \
		src/libc/string.c \
		src/libc/time.c \
		src/libc/errno.c \
		user/libc/stdlib.c \
	-o test/test_buddy_allocator.c.bin ; \
	echo "Testing test/test_buddy_allocator.c" ; \
	test/test_buddy_allocator.c.bin ; \
	rc=$$?; \
	rm test/test_buddy_allocator.c.bin ; \
	if [ $$rc -ne 0 ]; then exit $$rc; fi;

	# Direct allocation tests require user/libc/stdlib.c to exercise custom alignment logic.
	gcc $(GCOV_FLAGS) -std=gnu11 -DMENIOS_NO_DEBUG -DMENIOS_HOST_TEST -DUNITY_EXCLUDE_SETJMP_H -I./include \
		test/test_malloc_direct.c \
		test/unity.c \
		test/stubs.c \
		src/kernel/file.c \
		src/kernel/fs/vfs/vfs.c \
		src/kernel/fs/core/pipe.c \
		src/kernel/fs/tmpfs/tmpfs.c \
	src/kernel/fs/devfs/devfs.c \
		src/kernel/syscall/syscall.c \
			src/kernel/syscall/entry.c \
		src/kernel/mem/pmm.c \
		src/kernel/console/vprintk.c \
	src/kernel/console/ansi.c \
	src/kernel/proc/kcondvar.c \
	src/kernel/proc/kmutex.c \
	src/kernel/proc/signal.c \
	src/kernel/ipc/shm.c \
	src/kernel/user/vm_region.c \
	src/kernel/timer/tsc.c \
	src/kernel/block/block_cache.c \
	test/stubs_framebuffer.c \
	test/stubs_fat32.c \
	src/libc/itoa.c \
	src/libc/string.c \
	src/libc/time.c \
src/libc/errno.c \
		user/libc/stdlib.c \
	-o test/test_malloc_direct.c.bin ; \
	echo "Testing test/test_malloc_direct.c" ; \
	test/test_malloc_direct.c.bin ; \
	rc=$$?; \
	rm test/test_malloc_direct.c.bin ; \
	if [ $$rc -ne 0 ]; then exit $$rc; fi;

	# Host-stubbed malloc stats test ensures diagnostics stay consistent.
	gcc $(GCOV_FLAGS) -std=gnu11 -DMENIOS_NO_DEBUG -DMENIOS_HOST_TEST -DUNITY_EXCLUDE_SETJMP_H -I./include \
		test/test_malloc_stats.c \
		test/unity.c \
		test/stubs.c \
		src/kernel/file.c \
		src/kernel/fs/vfs/vfs.c \
		src/kernel/fs/core/pipe.c \
		src/kernel/fs/tmpfs/tmpfs.c \
	src/kernel/fs/devfs/devfs.c \
		src/kernel/syscall/syscall.c \
			src/kernel/syscall/entry.c \
		src/kernel/mem/pmm.c \
		src/kernel/console/vprintk.c \
		src/kernel/console/ansi.c \
		src/kernel/proc/kcondvar.c \
		src/kernel/proc/kmutex.c \
		src/kernel/proc/signal.c \
		src/kernel/ipc/shm.c \
		src/kernel/user/vm_region.c \
		src/kernel/timer/tsc.c \
		src/kernel/block/block_cache.c \
		src/libc/itoa.c \
		src/libc/string.c \
		src/libc/time.c \
		src/libc/errno.c \
		test/stubs_framebuffer.c \
		test/stubs_fat32.c \
		user/libc/stdlib.c \
	-o test/test_malloc_stats.c.bin ; \
	echo "Testing test/test_malloc_stats.c" ; \
	test/test_malloc_stats.c.bin ; \
	rc=$$?; \
	rm test/test_malloc_stats.c.bin ; \
	if [ $$rc -ne 0 ]; then exit $$rc; fi;

	# system() tests validate shell invocation semantics.
	gcc -std=gnu11 -DMENIOS_NO_DEBUG -DMENIOS_HOST_TEST -DUNITY_EXCLUDE_SETJMP_H -I./include \
		test/test_system.c \
		test/unity.c \
		test/stubs.c \
		src/kernel/file.c \
		src/kernel/fs/vfs/vfs.c \
		src/kernel/fs/core/pipe.c \
		src/kernel/fs/tmpfs/tmpfs.c \
	src/kernel/fs/devfs/devfs.c \
		src/kernel/syscall/syscall.c \
			src/kernel/syscall/entry.c \
		src/kernel/mem/pmm.c \
		src/kernel/console/vprintk.c \
		src/kernel/console/ansi.c \
		src/kernel/proc/kcondvar.c \
		src/kernel/proc/kmutex.c \
		src/kernel/proc/signal.c \
		src/kernel/ipc/shm.c \
		src/kernel/user/vm_region.c \
		src/kernel/timer/tsc.c \
		src/kernel/block/block_cache.c \
		test/stubs_framebuffer.c \
		test/stubs_fat32.c \
		src/libc/itoa.c \
		src/libc/string.c \
		src/libc/time.c \
		src/libc/errno.c \
		user/libc/stdlib.c \
	-o test/test_system.c.bin ; \
	echo "Testing test/test_system.c" ; \
	test/test_system.c.bin ; \
	rc=$$?; \
	rm test/test_system.c.bin ; \
	if [ $$rc -ne 0 ]; then exit $$rc; fi;

	# Kernel heap virtual range accounting tests need kmalloc.
	gcc $(GCOV_FLAGS) -std=gnu11 -DMENIOS_NO_DEBUG -DMENIOS_HOST_TEST -DUNITY_EXCLUDE_SETJMP_H -I./include \
		test/test_heap_virtual.c \
		test/unity.c \
		test/stubs.c \
		src/kernel/file.c \
		src/kernel/fs/vfs/vfs.c \
		src/kernel/fs/core/pipe.c \
		src/kernel/fs/tmpfs/tmpfs.c \
	src/kernel/fs/devfs/devfs.c \
		src/kernel/syscall/syscall.c \
			src/kernel/syscall/entry.c \
		src/kernel/mem/pmm.c \
		src/kernel/console/vprintk.c \
		src/kernel/console/ansi.c \
		src/kernel/proc/kcondvar.c \
		src/kernel/proc/kmutex.c \
		src/kernel/proc/signal.c \
		src/kernel/ipc/shm.c \
		src/kernel/user/vm_region.c \
		src/kernel/timer/tsc.c \
		src/kernel/mem/kmalloc.c \
		src/kernel/block/block_cache.c \
		src/libc/itoa.c \
		src/libc/string.c \
		src/libc/time.c \
		src/libc/errno.c \
		test/stubs_framebuffer.c \
		test/stubs_fat32.c \
		user/libc/stdlib.c \
	-o test/test_heap_virtual.c.bin ; \
	echo "Testing test/test_heap_virtual.c" ; \
	test/test_heap_virtual.c.bin ; \
	rc=$$?; \
	rm test/test_heap_virtual.c.bin ; \
	if [ $$rc -ne 0 ]; then exit $$rc; fi;

	# Host-stubbed allocator stress test exercises user/libc/stdlib.c explicitly.
	gcc $(GCOV_FLAGS) -std=gnu11 -DMENIOS_NO_DEBUG -DMENIOS_HOST_TEST -DUNITY_EXCLUDE_SETJMP_H -I./include \
		test/test_malloc_stress.c \
		test/unity.c \
		test/stubs.c \
		src/kernel/file.c \
		src/kernel/fs/vfs/vfs.c \
		src/kernel/fs/core/pipe.c \
		src/kernel/fs/tmpfs/tmpfs.c \
	src/kernel/fs/devfs/devfs.c \
		src/kernel/syscall/syscall.c \
			src/kernel/syscall/entry.c \
		src/kernel/mem/pmm.c \
		src/kernel/console/vprintk.c \
	src/kernel/console/ansi.c \
	src/kernel/proc/kcondvar.c \
	src/kernel/proc/kmutex.c \
	src/kernel/proc/signal.c \
	src/kernel/ipc/shm.c \
	src/kernel/user/vm_region.c \
	src/kernel/timer/tsc.c \
	src/kernel/block/block_cache.c \
	src/libc/itoa.c \
	src/libc/string.c \
	src/libc/time.c \
	src/libc/errno.c \
	test/stubs_framebuffer.c \
	test/stubs_fat32.c \
	user/libc/stdlib.c \
-o test/test_malloc_stress.c.bin ; \
	echo "Testing test/test_malloc_stress.c" ; \
	test/test_malloc_stress.c.bin ; \
	rc=$$?; \
	rm test/test_malloc_stress.c.bin ; \
	if [ $$rc -ne 0 ]; then exit $$rc; fi;

	# scanf family tests exercise formatted input helpers.
	gcc $(GCOV_FLAGS) -std=gnu11 -DMENIOS_NO_DEBUG -DMENIOS_HOST_TEST -DUNITY_EXCLUDE_SETJMP_H -I./include \
		test/test_scanf.c \
		user/libc/stdio.c \
		user/libc/stdlib.c \
		src/libc/string.c \
		src/libc/errno.c \
		src/libc/time.c \
		test/stubs_framebuffer.c \
		test/stubs_fat32.c \
	-o test/test_scanf.c.bin ; \
	echo "Testing test/test_scanf.c" ; \
	test/test_scanf.c.bin ; \
	rc=$$?; \
	rm test/test_scanf.c.bin ; \
	if [ $$rc -ne 0 ]; then exit $$rc; fi;
else
	@echo "Testing inside Docker"
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) $(DOCKER_ENV) --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make test"
endif

.PHONY: coverage coverage-report
coverage:
	@rm -rf $(GCOV_DIR)
	@mkdir -p $(GCOV_DIR)
	$(MAKE) GCOV=1 GCOV_DIR=$(GCOV_DIR) test
	$(MAKE) GCOV=1 GCOV_DIR=$(GCOV_DIR) coverage-report
	@find . -name '*.gcda' -delete || true
	@find . -name '*.gcno' -delete || true
	@find . -name '*.gcov' -delete || true
	@find $(GCOV_DIR) -name '*.gcov.json.gz' -delete || true

coverage-report:
ifeq ($(OS_NAME),linux)
	@./scripts/gcov-report.sh $(GCOV_DIR)
else
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) $(DOCKER_ENV) --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && ./scripts/gcov-report.sh $(GCOV_DIR)"
endif

.PHONY: shell
shell:
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) $(DOCKER_ENV) --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && /bin/bash"

.PHONY: doom
doom: sdk
ifeq ($(OS_NAME),linux)
	$(MAKE) -C vendor/genericdoom -f Makefile.menios
else
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) $(DOCKER_ENV) --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make sdk && make -C vendor/genericdoom -f Makefile.menios"
endif

.PHONY: build-apps
build-apps:

TEMP_DISABLE_SUPERVISION_FLAGS = -DTEMP_DISABLE_SUPERVISION

build-temp-disable:
	$(MAKE) EXTRA_CFLAGS=${TEMP_DISABLE_SUPERVISION_FLAGS} build

.PHONY: get-doom-wad
get-doom-wad:
	@echo "Preparing IWADs..."
	@if [ ! -f $(OUTPUT_DIR)/doom1.wad ]; then \
		mkdir -p $(OUTPUT_DIR); \
		curl -L https://distro.ibiblio.org/slitaz/sources/packages/d/doom1.wad -o $(OUTPUT_DIR)/doom1.wad; \
	fi
	@if [ ! -f $(OUTPUT_DIR)/doom2.wad ]; then \
		mkdir -p $(OUTPUT_DIR); \
		curl -L https://www.pc-freak.net/files/doom-wad-files/Doom2.wad -o $(OUTPUT_DIR)/doom2.wad; \
	fi
ifneq ($(strip $(DOOM1_WAD)),)
	@if [ -f "$(DOOM1_WAD)" ]; then \
		echo "Copying DOOM II IWAD from $(DOOM1_WAD)"; \
		mkdir -p $(OUTPUT_DIR); \
		cp "$(DOOM1_WAD)" $(OUTPUT_DIR)/doom2.wad; \
	else \
		echo "warn: DOOM1_WAD='$(DOOM1_WAD)' not found, skipping"; \
	fi
else
	@echo "(optional) Set DOOM1_WAD=/path/to/DOOM1.WAD before make get-doom-wad to bundle DOOM I"
endif

ifneq ($(strip $(DOOM2_WAD)),)
	@if [ -f "$(DOOM2_WAD)" ]; then \
		echo "Copying DOOM II IWAD from $(DOOM2_WAD)"; \
		mkdir -p $(OUTPUT_DIR); \
		cp "$(DOOM2_WAD)" $(OUTPUT_DIR)/doom2.wad; \
	else \
		echo "warn: DOOM2_WAD='$(DOOM2_WAD)' not found, skipping"; \
	fi
else
	@echo "(optional) Set DOOM2_WAD=/path/to/DOOM2.WAD before make get-doom-wad to bundle DOOM II"
endif

.PHONY: binutils binutils-host
binutils: 
ifeq ($(OS_NAME),linux)
	$(MAKE) binutils-host
else
	$(MAKE) docker
	$(DOCKER) run --rm $(DOCKER_RUN_FLAGS) $(DOCKER_ENV) --platform linux/amd64 --mount type=bind,source=$$(pwd),target=/mnt $(DOCKER_IMAGE) /bin/sh -c "cd /mnt && make binutils-host"
	$(MAKE) binutils-native
endif

binutils-host: userland $(BINUTILS_BUILD_DIR)/Makefile
	MENIOS_HOST_BUILD=1 MENIOS_HOST_CC=gcc MENIOS_HOST_AR=ar MENIOS_HOST_RANLIB=ranlib MENIOS_ENABLE_SSE=1 AR=$(abspath tools/menios-ar.sh) RANLIB=$(abspath tools/menios-ranlib.sh) $(MAKE) -C $(BINUTILS_BUILD_DIR) MAKEINFO=true
	MENIOS_HOST_BUILD=1 MENIOS_HOST_CC=gcc MENIOS_HOST_AR=ar MENIOS_HOST_RANLIB=ranlib MENIOS_ENABLE_SSE=1 AR=$(abspath tools/menios-ar.sh) RANLIB=$(abspath tools/menios-ranlib.sh) $(MAKE) -C $(BINUTILS_BUILD_DIR) MAKEINFO=true install

$(BINUTILS_BUILD_DIR)/Makefile: userland
	rm -rf $(BINUTILS_BUILD_DIR)
	mkdir -p $(BINUTILS_BUILD_DIR)
	cd $(BINUTILS_BUILD_DIR) && \
		MENIOS_HOST_BUILD=1 \
		MENIOS_HOST_CC=gcc \
		MENIOS_HOST_AR=ar \
		MENIOS_HOST_RANLIB=ranlib \
		MENIOS_ENABLE_SSE=1 \
		MENIOS_SDK_ROOT=$(BINUTILS_PREFIX) \
		CC=$(abspath tools/menios-gcc.sh) \
		AR=$(abspath tools/menios-ar.sh) \
		RANLIB=$(abspath tools/menios-ranlib.sh) \
		ac_cv_header_stdio_ext_h=no \
		$(abspath $(BINUTILS_SRC_DIR))/configure \
		  --target=x86_64-menios \
		  --prefix=$(BINUTILS_PREFIX) \
		  $(BINUTILS_CONFIGURE_FLAGS)

.PHONY: binutils-native
binutils-native: sdk $(BINUTILS_NATIVE_BUILD_DIR)/Makefile
	MENIOS_ENABLE_SSE=1 MENIOS_SDK_ROOT=$(BINUTILS_PREFIX) $(MAKE) -C $(BINUTILS_NATIVE_BUILD_DIR) MAKEINFO=true
	MENIOS_ENABLE_SSE=1 MENIOS_SDK_ROOT=$(BINUTILS_PREFIX) $(MAKE) -C $(BINUTILS_NATIVE_BUILD_DIR) MAKEINFO=true install

$(BINUTILS_NATIVE_BUILD_DIR)/Makefile: sdk
	rm -rf $(BINUTILS_NATIVE_BUILD_DIR)
	mkdir -p $(BINUTILS_NATIVE_BUILD_DIR)
	cd $(BINUTILS_NATIVE_BUILD_DIR) && \
		MENIOS_SDK_ROOT=$(BINUTILS_PREFIX) \
		MENIOS_ENABLE_SSE=1 \
		CC=$(abspath tools/menios-gcc.sh) \
		AR=$(abspath tools/menios-ar.sh) \
		RANLIB=$(abspath tools/menios-ranlib.sh) \
		bu_cv_header_utime_h=yes \
		ac_cv_header_utime_h=yes \
		ac_cv_func_utime=yes \
		ac_cv_func_fchmod=yes \
		ac_cv_func_isatty=yes \
		ac_cv_func_strcspn=yes \
		ac_cv_func_strspn=yes \
		ac_cv_tls=none \
		$(abspath $(BINUTILS_SRC_DIR))/configure \
		  --host=x86_64-menios \
		  --target=x86_64-menios \
		  --prefix=$(BINUTILS_PREFIX) \
		  $(BINUTILS_CONFIGURE_FLAGS) \
		  --with-zstd=no
