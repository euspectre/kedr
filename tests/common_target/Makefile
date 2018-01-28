module_name = kedr_common_target

KBUILD_DIR ?= /lib/modules/$(shell uname -r)/build
PWD = $(shell pwd)

all: ${module_name}.ko

${module_name}.ko: cfake.c cfake.h kedr_stubs.c
	$(MAKE) -C ${KBUILD_DIR} M=${PWD} modules

clean:
	$(MAKE) -C ${KBUILD_DIR} M=${PWD} clean

.PHONY: all clean
