MODULE_NAME := cr_watcher

# This subdirectory contains necessary files both for x86 and x86_64.
ARCH_DIR := arch/x86

# This auxiliary file will be generated during the build (x86 instruction 
# tables as C code).
INAT_TABLES_FILE := inat-tables.c

ifeq ($(KERNELRELEASE),)
# 'Out-of-kernel' part
# It is used in the first place when the user executes 'make [target]'.
# Eventually, it will invoke the kernel build system and that, in turn, will
# execute the 'KBuild' part of this Makefile
# (see "$(MAKE) -C $(KERNELDIR) M=$(PWD)" stuff).

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

modules_install:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules_install

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c \
		.tmp_versions *.order *.symvers *.markers \
		$(ARCH_DIR)/lib/*.o $(ARCH_DIR)/lib/.*.cmd \
		$(ARCH_DIR)/lib/$(INAT_TABLES_FILE)

.PHONY: modules modules_install clean

else
# KBuild part. 
# It is used by the kernel build system to actually build the module.

# -I$(src) is necessary for the event tracing facility to work properly.
# -I...$(ARCH_DIR)... options are necessary for the instruction decoder.
ccflags-y := -I$(src)/$(ARCH_DIR)/include -I$(obj)/$(ARCH_DIR)/lib \
		-I$(src)

obj-m := $(MODULE_NAME).o
$(MODULE_NAME)-y := \
	cfake.o \
	$(ARCH_DIR)/lib/inat.o \
	$(ARCH_DIR)/lib/insn.o

$(obj)/$(ARCH_DIR)/lib/inat.o: $(obj)/$(ARCH_DIR)/lib/$(INAT_TABLES_FILE) $(src)/$(ARCH_DIR)/lib/inat.c

# Note "LC_ALL=C" before awk, it is necessary. Without it, the awk script 
# will report semantic error in the input file on some systems. This is 
# probably due to some locale-related issues that affect separator characters,
# string parsing, etc.
$(obj)/$(ARCH_DIR)/lib/$(INAT_TABLES_FILE): $(src)/$(ARCH_DIR)/lib/x86-opcode-map.txt 
	LC_ALL=C awk -f $(src)/$(ARCH_DIR)/tools/gen-insn-attr-x86.awk $< > $@

endif
