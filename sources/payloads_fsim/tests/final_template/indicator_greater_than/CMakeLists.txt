kbuild_add_module(${indicator_greater_than_module_name} indicator.c)
kbuild_link_module(${indicator_greater_than_module_name} kedr_fault_simulation)


kedr_test_add_target(${indicator_greater_than_module_name})