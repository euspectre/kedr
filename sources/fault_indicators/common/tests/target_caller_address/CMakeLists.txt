set(kmodule_name "target_caller_address")

kbuild_add_module(${kmodule_name} "target.c")

kedr_test_add_target(${kmodule_name})