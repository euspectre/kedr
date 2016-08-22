# [NB] All the modules (except the ones for testing) should probably go here,
# into this directory. May simplify dealing with symvers as well.

ccflags-y := -I$(src)

obj-$(CONFIG_KEDR) += kedr.o
kedr-objs := core.o

# <module_name>-${CONFIG_KEDR_LEAK_CHECKER} += ...
# <module_name>-${CONFIG_KEDR_RACE_DETECTOR} += ...
