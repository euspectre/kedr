set(kmodule_name simple_ordering_module)

kbuild_add_module(${kmodule_name} "module.c")
kbuild_link_module(${kmodule_name} kedr_trace)

kedr_test_add_target(${kmodule_name})