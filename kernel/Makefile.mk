# Override KBUILD_DIR when calling make with this Makefile to build
# everything for a different kernel.
KBUILD_DIR ?= /lib/modules/$(shell uname -r)/build

TARGET_KERNEL := $(shell $(MAKE) -s -C $(KBUILD_DIR) kernelversion)

BUILD_DIR := $(shell pwd)

ifeq ($(ARCH),)
	HDR_ARCH := $(shell uname -m)
	ARCH_SPEC :=
else
	HDR_ARCH := $(ARCH)
	ARCH_SPEC := "ARCH=$(ARCH)"
endif

ifeq ($(CROSS_COMPILE),)
	CROSS_COMPILE_SPEC :=
else
	CROSS_COMPILE_SPEC := "CROSS_COMPILE=$(CROSS_COMPILE)"
endif

# Similar to SUBARCH from the Makefile for the kernel.
HDR_ARCH := $(shell echo "$(HDR_ARCH)" | sed \
				 -e s/i.86/x86/ -e s/x86_64/x86/ \
				 -e s/sun4u/sparc64/ \
				 -e s/arm.*/arm/ -e s/sa110/arm/ \
				 -e s/s390x/s390/ -e s/parisc64/parisc/ \
				 -e s/ppc.*/powerpc/ -e s/mips.*/mips/ \
				 -e s/sh[234].*/sh/ -e s/aarch64.*/arm64/ )

ARCH_INCLUDE_PATH := $(BUILD_DIR)/arch/$(HDR_ARCH)/include
# GCC processes "-include <file>" as if #include "<file>" was at the first
# line of the primary source file. No need to #include it explicitly, which
# is convenient.
KCFLAGS += -I$(BUILD_DIR)/include -I$(ARCH_INCLUDE_PATH) -include $(BUILD_DIR)/config.h
export KCFLAGS
############################################################################

CORE_SUBDIR := kernel/kedr

# The kernel modules to be built.
MODULES := \
	$(CORE_SUBDIR)/kedr.ko

all: $(MODULES)

$(CORE_SUBDIR)/kedr.ko:
	$(MAKE)$(ARCH_SPEC)$(CROSS_COMPILE_SPEC) -C $(KBUILD_DIR) M=$(BUILD_DIR)/$(CORE_SUBDIR) modules

clean:
	$(MAKE)$(ARCH_SPEC)$(CROSS_COMPILE_SPEC) -C $(KBUILD_DIR) M=$(BUILD_DIR)/$(CORE_SUBDIR) clean

# No need to meddle with installation of the built modules here:
# either CMake (in the local builds) or the package/DKMS/whatever should
# handle it.

.PHONY: all clean $(MODULES)
