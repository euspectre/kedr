kbuild_add_module("kedr_calc_test_weak_vars" "module.c" "calculator.c")
rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/calculator.c" "${CMAKE_CURRENT_BINARY_DIR}/../../calculator.c")

kedr_test_add_target("kedr_calc_test_weak_vars")

configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
  "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
  @ONLY
)
#Common usecase
kedr_test_add_script ("fault_simulation.calculator.weak_vars.01"
   "test.sh" "sub_expr + 100" "2*2" "104"
)
# If weak war is not used in expression, it shouldn't be computed
kedr_test_add_script ("fault_simulation.calculator.weak_vars.02"
   "test.sh" "300 + 100" "2*2*" "400"
)
