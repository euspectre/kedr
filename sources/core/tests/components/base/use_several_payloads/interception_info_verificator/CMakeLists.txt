kbuild_use_symbols("${CMAKE_CURRENT_BINARY_DIR}/../Module.symvers")
kbuild_add_module(${verificator_module_name}
    "verificator.c"
)

kedr_test_add_target(${verificator_module_name})