MODULE_NAME := cp_payload2

# The payload module uses the symbols exported by the controller module and
# hence, needs appropriate .symvers file. 
# This file will be copied to the current directory as "controller.symvers"
#
# If KEDR_CONTROLLER_SYMBOLS is not specified when calling make, the default
# path will be used.
KEDR_CONTROLLER_SYMVERS ?= ../cp_controller/Module.symvers

# Some kernels (e.g. 2.6.27) seem not to support KBUILD_EXTRA_SYMBOLS, so 
# we have to resort to the older approach and name the .symvers file from 
# the controller as 'Module.symvers' too.
#KEDR_CONTROLLER_SYMVERS_NAME := controller.symvers
KEDR_CONTROLLER_SYMVERS_NAME := Module.symvers

ifeq ($(KERNELRELEASE),)
# 'Out-of-kernel' part
# It is used in the first place when the user executes 'make [target]'.
# Eventually, it will invoke the kernel build system and that, in turn, will
# execute the 'KBuild' part of this Makefile
# (see "$(MAKE) -C $(KERNELDIR) M=$(PWD)" stuff).

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

modules: $(KEDR_CONTROLLER_SYMVERS)
	cp "$(KEDR_CONTROLLER_SYMVERS)" $(KEDR_CONTROLLER_SYMVERS_NAME)
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

modules_install:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules_install

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c \
		.tmp_versions *.order *.markers *.symvers

.PHONY: modules modules_install clean

else
# KBuild part. 
# It is used by the kernel build system to actually build the module.

#KBUILD_EXTRA_SYMBOLS := $(src)/$(KEDR_CONTROLLER_SYMVERS_NAME)

# This is necessary for "#include <cp_controller/controller_common.h>" to
# work.
# This include directory should be changed approptiately in production code
# where the payload modules are built independent on the controller.
ccflags-y := -I$(src)/..

obj-m := $(MODULE_NAME).o
$(MODULE_NAME)-y := \
	cp_payload.o
	
endif
