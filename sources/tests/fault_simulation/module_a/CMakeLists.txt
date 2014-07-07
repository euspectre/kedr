kbuild_add_module("fsim_test_module_a" "module_a.c")
kbuild_link_module("fsim_test_module_a" kedr_fault_simulation)

kedr_test_install_module("fsim_test_module_a")