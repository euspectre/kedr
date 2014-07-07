set(kmodule_name "kedr_indicator_common_test_module")

kbuild_add_module(${kmodule_name} "module.c")

kbuild_link_module(${kmodule_name} kedr_fault_simulation)

kedr_test_install_module(${kmodule_name})
