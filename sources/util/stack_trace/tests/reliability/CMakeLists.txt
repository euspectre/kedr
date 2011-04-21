# Test module
set(KMODULE_NAME "test_stack_trace")
# '@ONLY' is essential when doing substitutions in the shell scripts. 
# Without it, CMake would replace "${...}" too, which is usually not what 
# you want.
configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
  "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
  @ONLY
)

kedr_test_add_script (stack_trace.01 
    test.sh
)

rule_copy_file("stack_trace.c"
	"${kedr_stack_trace_source_dir}/stack_trace.c") 

kbuild_add_module(${KMODULE_NAME} 
    "test_module.c"
	"stack_trace.c"
)

kedr_test_add_target (${KMODULE_NAME})
