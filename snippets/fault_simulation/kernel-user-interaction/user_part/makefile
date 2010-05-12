# Create dynamic library, which provide managing
# of fault simulation scenarios.

SYSCALL_CONNECTOR_LIBRARY_PATH ?= $(HOME)/linux-modules
SYSCALL_CONNECTOR_LIBRARY_VERSION_MAJOR ?= 0

FSIM_LIB_NAME := fault_simulation
FSIM_VERSION_MAJOR := 0
FSIM_VERSION_MINOR := 1
FSIM_VERSION_MICRO := 0

FSIM_VERSION := $(FSIM_VERSION_MAJOR).$(FSIM_VERSION_MINOR).$(FSIM_VERSION_MICRO)

FSIM_LIB_FILE := lib$(FSIM_LIB_NAME).so.$(FSIM_VERSION)
FSIM_LIB_SONAME := lib$(FSIM_LIB_NAME).so.$(FSIM_VERSION_MAJOR)

VERSION_SCRIPT=versions.ldscript

FSIM_OBJS := fault_simulation.o

all: $(FSIM_LIB_FILE)


$(FSIM_LIB_FILE): $(FSIM_OBJS)
	gcc --shared \
	-Wl,-soname,$(FSIM_LIB_SONAME) \
	-Wl,-z,-defs -o $@ $^ -lc \
	-Wl,--version-script=$(VERSION_SCRIPT) \
	-Wl,-rpath,$(SYSCALL_CONNECTOR_LIBRARY_PATH)/syscall_connector/user_part \
	$(SYSCALL_CONNECTOR_LIBRARY_PATH)/syscall_connector/user_part/libsyscall_connector.so.$(SYSCALL_CONNECTOR_LIBRARY_VERSION_MAJOR)
	ln -s -f $(FSIM_LIB_FILE) $(FSIM_LIB_SONAME)
fault_simulation.o: fault_simulation.c
	gcc -I$(SYSCALL_CONNECTOR_LIBRARY_PATH) -std=gnu99 -O2 -Wall -Wextra -fPIC -c -o $@ $^
clean:
	rm -f $(FSIM_LIB_SONAME) $(FSIM_LIB_FILE) $(FSIM_OBJS)

.PHONY: all clean