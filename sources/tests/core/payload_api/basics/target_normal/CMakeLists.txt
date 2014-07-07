set(KMODULE_NAME "test_target_normal")

kbuild_include_directories ("${CMAKE_CURRENT_SOURCE_DIR}")
kbuild_add_module(${KMODULE_NAME} 
    "target.c"
)

kedr_test_install_module (${KMODULE_NAME})
