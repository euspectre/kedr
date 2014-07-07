kbuild_add_module("kedr_calc_test_var_names" "module.c" "calculator.c")
rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/calculator.c" "${CALCULATOR_SOURCE_DIR}/calculator.c")

kedr_test_install_module("kedr_calc_test_var_names")

configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
  "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
  @ONLY
)

kedr_test_install(PROGRAMS "test.sh")

# One name is substring if another
kedr_test_add_script_shared ("fault_simulation.calculator.var_names.01"
   "test.sh" "a + 100" "aa" "a" "102"
)
kedr_test_add_script_shared ("fault_simulation.calculator.var_names.02"
   "test.sh" "a + 100" "a" "aa" "101"
)
# Name contain digits
kedr_test_add_script_shared ("fault_simulation.calculator.var_names.03"
   "test.sh" "a0123+100" "a0123" "a012" "101"
)
#Unknown name - superstring
kedr_test_add_script_shared ("fault_simulation.calculator.var_names.04"
   "test.sh" "a0123" "a" "b"
)
#Unknown name - substring
kedr_test_add_script_shared ("fault_simulation.calculator.var_names.05"
   "test.sh" "ab0" "ab" "ab00"
)
#Incorrect name
kedr_test_add_script_shared ("fault_simulation.calculator.var_names.06"
   "test.sh" "12ab0" "ab0" "ab00"
)
