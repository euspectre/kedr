kbuild_add_module("fsim_test_module_b" "module_b.c")
kbuild_link_module("fsim_test_module_b" kedr_fault_simulation)

kedr_test_install_module("fsim_test_module_b")