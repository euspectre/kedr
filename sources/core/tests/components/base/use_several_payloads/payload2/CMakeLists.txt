kbuild_use_symbols("${CMAKE_CURRENT_BINARY_DIR}/../Module.symvers")
kbuild_add_module(${payload_module2_name}
    "payload.c"
)

kedr_test_add_target(${payload_module2_name})