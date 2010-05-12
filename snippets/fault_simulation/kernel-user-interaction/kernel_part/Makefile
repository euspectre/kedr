MODULE_NAME := fault_indicator_manager
#dependencies
KEDR_CONTROLLER_REL_DIR ?= ../../../controller_payload
SYSCALL_CONNECTOR_REL_DIR ?= ../../..
KEDR_FAULT_SIMULATION_REL_DIR ?= ../../..
KEDR_MODULE_WEAK_REF_SOURCES ?= $(KEDR_FAULT_SIMULATION_REL_DIR)/fault_simulation
# The payload module uses the symbols exported by the controller module and
# hence, needs appropriate .symvers file. 
# This file will be copied to the current directory as "controller.symvers"
#
# If KEDR_FAULT_SIMULATION_SYMVERS is not specified when calling make, the default
# path will be used.
KEDR_FAULT_SIMULATION_SYMVERS ?= $(KEDR_FAULT_SIMULATION_REL_DIR)/fault_simulation/Module.symvers
KEDR_CONTROLLER_SYMVERS ?= $(KEDR_CONTROLLER_REL_DIR)/cp_controller/Module.symvers
SYSCALL_CONNECTOR_SYMVERS ?= $(SYSCALL_CONNECTOR_REL_DIR)/syscall_connector/Module.symvers
# Some kernels (e.g. 2.6.27) seem not to support KBUILD_EXTRA_SYMBOLS, so 
# we have to resort to the older approach and name the .symvers file from 
# the controller as 'Module.symvers' too.

KEDR_SYMVERS_NAME := Module.symvers

ifeq ($(KERNELRELEASE),)
# 'Out-of-kernel' part
# It is used in the first place when the user executes 'make [target]'.
# Eventually, it will invoke the kernel build system and that, in turn, will
# execute the 'KBuild' part of this Makefile
# (see "$(MAKE) -C $(KERNELDIR) M=$(PWD)" stuff).

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

modules: $(KEDR_SYMVERS_NAME) module_weak_ref.c module_weak_ref.h
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

module_weak_ref.c module_weak_ref.h: %: $(KEDR_MODULE_WEAK_REF_SOURCES)/%
	cp $^ $@

modules_install:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules_install

$(KEDR_SYMVERS_NAME): $(KEDR_FAULT_SIMULATION_SYMVERS) $(KEDR_CONTROLLER_SYMVERS) $(SYSCALL_CONNECTOR_SYMVERS)
	cat $^ > $@

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c \
		.tmp_versions *.order *.markers *.symvers
	rm -f module_weak_ref.c module_weak_ref.h
.PHONY: modules modules_install clean

else
# KBuild part. 
# It is used by the kernel build system to actually build the module.

#KBUILD_EXTRA_SYMBOLS := $(src)/$(KEDR_CONTROLLER_SYMVERS_NAME)

# This is necessary for "#include <fault_simulation/fault_simulation.h>" to
# work.
ccflags-y := -I$(src)/$(KEDR_FAULT_SIMULATION_REL_DIR) \
-I$(src)/$(KEDR_CONTROLLER_REL_DIR) \
-I$(src)/$(SYSCALL_CONNECTOR_REL_DIR)

obj-m := $(MODULE_NAME).o
$(MODULE_NAME)-objs := \
	fault_indicator_manage.o module_weak_ref.o
	
endif
