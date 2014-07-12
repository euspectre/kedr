if(KERNEL_PART)
    kbuild_add_module("kedr_calc_test_weak_vars" "module.c" "calculator.c")
    rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/calculator.c" "${CALCULATOR_SOURCE_DIR}/calculator.c")

    kedr_test_install_module("kedr_calc_test_weak_vars")
endif(KERNEL_PART)
if(NOT USER_PART)
    return()
endif(NOT USER_PART)

configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
  "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
  @ONLY
)

kedr_test_install(PROGRAMS "test.sh")

#Common usecase
kedr_test_add_script_shared ("fault_simulation.calculator.weak_vars.01"
   "test.sh" "sub_expr + 100" "2*2" "104"
)
# If weak war is not used in expression, it shouldn't be computed
kedr_test_add_script_shared ("fault_simulation.calculator.weak_vars.02"
   "test.sh" "300 + 100" "2*2*" "400"
)
