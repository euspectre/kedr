set(KMODULE_NAME "test_in_init_target")

kbuild_include_directories ("${CMAKE_CURRENT_SOURCE_DIR}")
kbuild_add_module(${KMODULE_NAME} 
    "test_in_init_target.c"
)

kedr_test_add_target (${KMODULE_NAME})
