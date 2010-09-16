kbuild_use_symbols("${CMAKE_CURRENT_BINARY_DIR}/../../Module.symvers")
kbuild_add_module(fsim_im_test_simple module.c)

kedr_test_add_target(fsim_im_test_simple)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
    "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
    @ONLY
)

kedr_test_add_script("fault_simulation.indicator_manager.simple.01"
    test.sh)