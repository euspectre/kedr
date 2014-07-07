set(KMODULE_NAME "test_in_init_target")

kbuild_add_module(${KMODULE_NAME} 
    "test_in_init_target.c"
)

kedr_test_install_module (${KMODULE_NAME})
