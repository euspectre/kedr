kbuild_add_module(${indicator_constant_module_name} indicator.c)
kbuild_link_module(${indicator_constant_module_name} kedr_fault_simulation)

kedr_test_install_module(${indicator_constant_module_name})