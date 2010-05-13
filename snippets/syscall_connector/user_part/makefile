# Create dynamic library, which provide managing
# of fault simulation scenarios.

LIB_NAME := syscall_connector
VERSION_MAJOR := 0
VERSION_MINOR := 1
VERSION_MICRO := 0

VERSION := $(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_MICRO)

LIB_FILE := lib$(LIB_NAME).so.$(VERSION)
LIB_SONAME := lib$(LIB_NAME).so.$(VERSION_MAJOR)

VERSION_SCRIPT=versions.ldscript

CFLAGS := -std=gnu99 -O2 -Wall -Wextra -fPIC
LAFLAGS := -Wl,-soname,$(LIB_SONAME) -Wl,-z,-defs -lc -Wl,--version-script=$(VERSION_SCRIPT)

OBJS := syscall_connector.o

all: $(LIB_FILE) $(LIB_SONAME)

$(LIB_SONAME): $(LIB_FILE)
	ln -s -f $@ $^

$(LIB_FILE): $(OBJS)
	$(CC) --shared $(LAFLAGS) -o $@ $^ 

clean:
	rm -f $(LIB_SONAME) $(LIB_FILE) $(OBJS)

.PHONY: all clean