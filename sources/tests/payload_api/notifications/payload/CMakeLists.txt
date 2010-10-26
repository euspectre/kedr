set(KMODULE_NAME "test_notifications_payload")

kbuild_use_symbols("${CMAKE_BINARY_DIR}/base/Module.symvers")
kbuild_include_directories ("${CMAKE_CURRENT_SOURCE_DIR}")
kbuild_add_module(${KMODULE_NAME} 
    "payload.c"
)

kedr_test_add_target (${KMODULE_NAME})
