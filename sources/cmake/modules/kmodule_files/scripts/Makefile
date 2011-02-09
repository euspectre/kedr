MODULE_NAME := kedr_probe

ifeq ($(KERNELRELEASE),)
KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean

.PHONY: modules modules_install clean

else
# KBuild part. 

obj-m := $(MODULE_NAME).o
$(MODULE_NAME)-y := probe.o

endif
