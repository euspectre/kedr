MODULE_NAMES := netlink_test
CLEAN_MODULES := $(addprefix _clean_, $(MODULE_NAMES))

obj-m := $(addsuffix .o, $(MODULE_NAMES))

BUILD_DIR=/lib/modules/2.6.27-7-generic/build

all:
	make -C $(BUILD_DIR) M=`pwd` modules

clean: $(CLEAN_MODULES)
	rm -rf ./.tmp_versions
	rm -f Module.markers Module.symvers modules.order

$(CLEAN_MODULES): _clean_% :
	rm -f $(addprefix $*, .o .ko .mod.c .mod.o)
	rm -f $(addprefix .$*, .o.cmd .ko.cmd .mod.o.cmd)

.PHONY: all clean $(CLEAN_MODULES)