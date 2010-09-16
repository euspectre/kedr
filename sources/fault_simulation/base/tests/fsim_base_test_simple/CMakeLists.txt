kbuild_use_symbols("${CMAKE_BINARY_DIR}/fault_simulation/base/Module.symvers")
kbuild_add_module("kedr_fsim_base_test_simple" "fsim_base_test_simple.c")

kedr_test_add_target("kedr_fsim_base_test_simple")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in
    ${CMAKE_CURRENT_BINARY_DIR}/test.sh
    @ONLY
)

kedr_test_add_script(fault_simulation.base.simple.01
    test.sh)
