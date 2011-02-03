kbuild_add_module("kedr_calc_test_vars" "calc_test_vars.c" "calculator.c")
rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/calculator.c" "${CMAKE_CURRENT_SOURCE_DIR}/../../calculator.c")

kedr_test_add_target("kedr_calc_test_vars")

configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
  "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
  @ONLY
)
# Without constants
kedr_test_add_script ("fault_simulation.calculator.vars.01"
   "test.sh" "x+y" "2" "45" "47"
)
# Using constant
kedr_test_add_script ("fault_simulation.calculator.vars.02"
   "test.sh" "(x & y) = FALSE ? x * y : x - y" "17" "14" "238"
)
# Using constant with underscore ('_')
kedr_test_add_script ("fault_simulation.calculator.vars.03"
   "test.sh" "x >= big_number ? x + 1 : y -1" "9999" "45" "44"
)
