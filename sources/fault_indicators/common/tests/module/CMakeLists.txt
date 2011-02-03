set(kmodule_name "kedr_indicator_common_test_module")

kbuild_add_module(${kmodule_name} "module.c")

kedr_test_add_target(${kmodule_name})
