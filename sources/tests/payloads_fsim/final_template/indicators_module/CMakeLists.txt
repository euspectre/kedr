kbuild_add_module(${indicators_module_name} indicator.c)
kbuild_link_module(${indicators_module_name} kedr_fault_simulation)

kedr_test_install_module(${indicators_module_name})