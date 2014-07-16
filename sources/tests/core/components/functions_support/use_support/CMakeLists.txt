set(functions_support_module_name "functions_support_module_use_support_m")

rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/kedr_functions_support.c"
    "${CORE_SOURCE_DIR}/kedr_functions_support.c"
)

kbuild_add_module(${functions_support_module_name}
    "functions_support_module.c"
    "kedr_functions_support.c"
)

kedr_test_install_module(${functions_support_module_name})

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
    "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
    @ONLY
)

kedr_test_add_script("core.components.functions_support.use_support.01"
    "test.sh"
)

