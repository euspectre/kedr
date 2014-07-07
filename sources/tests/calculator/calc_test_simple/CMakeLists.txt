kbuild_add_module("kedr_calc_test_simple" "calc_test_simple.c" "calculator.c")
rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/calculator.c" "${CALCULATOR_SOURCE_DIR}/calculator.c")

kedr_test_install_module("kedr_calc_test_simple")

configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
  "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
  @ONLY
)

kedr_test_install(PROGRAMS "test.sh")

# Simple action
kedr_test_add_script_shared ("fault_simulation.calculator.simple.01"
   "test.sh" "2+2" "4"
)
# Priority
kedr_test_add_script_shared ("fault_simulation.calculator.simple.02"
   "test.sh" "2+2*22" "46"
)
# Parentheses
kedr_test_add_script_shared ("fault_simulation.calculator.simple.03"
   "test.sh" "(2+2)*22" "88"
)
# Operation ?:
kedr_test_add_script_shared ("fault_simulation.calculator.simple.04"
   "test.sh" "(321 = 321)? 1 : 22" "1"
)
# Operation ?: with non-atomic second and third arguments
kedr_test_add_script_shared ("fault_simulation.calculator.simple.05"
   "test.sh" "(2321<321)?2+4:54+5" "59"
)
# Operation ?: and grouping
kedr_test_add_script_shared ("fault_simulation.calculator.simple.06"
   "test.sh" "2 ? 4 : 0 ? 5 : 6" "4"
)

#Hexadecimal numbers - simple
kedr_test_add_script_shared ("fault_simulation.calculator.simple.07"
   "test.sh" "0x2+0x2" "4"
)

#Hexadecimal numbers
kedr_test_add_script_shared ("fault_simulation.calculator.simple.08"
   "test.sh" "0xaF +0X1Bd" "620"
)
