module_name = kedr_test_events

KBUILD_DIR ?= /lib/modules/$(shell uname -r)/build
PWD = $(shell pwd)

all: ${module_name}.ko

${module_name}.ko: test_events.c kedr_helpers.c
	$(MAKE) -C ${KBUILD_DIR} M=${PWD} modules

clean:
	$(MAKE) -C ${KBUILD_DIR} M=${PWD} clean

.PHONY: all clean
