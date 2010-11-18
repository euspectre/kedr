#name of module created
set(kmodule_name "kedr_indicator_internal_test_module")

kbuild_add_module(${kmodule_name} "module.c")

kedr_test_add_target(${kmodule_name})