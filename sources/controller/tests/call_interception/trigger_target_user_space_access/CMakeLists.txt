set(kmodule_name trigger_target_user_space_access)

kbuild_add_module(${kmodule_name} "target.c")

kedr_test_add_target(${kmodule_name})
