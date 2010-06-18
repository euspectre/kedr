set(KMODULE_NAME "simple_payload")

set(CONTROLLER_SYMVERS_FILE "${CMAKE_BINARY_DIR}/controller/Module.symvers")
kbuild_use_symbols(${CONTROLLER_SYMVERS_FILE})

kbuild_include_directories ("${CMAKE_SOURCE_DIR}")
kbuild_add_module(${KMODULE_NAME} 
    "simple_payload.c"
)

kedr_test_add_target (${KMODULE_NAME})
