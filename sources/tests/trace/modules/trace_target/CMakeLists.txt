set(kmodule_name ${TRACE_TEST_TARGET_MODULE_NAME})

kbuild_add_module(${kmodule_name} "module.c")
kbuild_link_module(${kmodule_name} kedr_trace ${TRACE_TEST_GENERATOR_MODULE_NAME})

kedr_test_install_module(${kmodule_name})