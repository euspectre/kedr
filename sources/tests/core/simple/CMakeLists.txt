# The stuff necessary for testing
add_subdirectory (simple_payload)

set (TARGET_NAME "kedr_sample_target")

# '@ONLY' is essential when doing substitutions in the shell scripts. 
# Without it, CMake would replace "${...}" too, which is usually not what 
# you want.
configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/ins_rm.sh.in"
  "${CMAKE_CURRENT_BINARY_DIR}/ins_rm.sh"
  @ONLY
)

configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/ins_rm_with_target.sh.in"
  "${CMAKE_CURRENT_BINARY_DIR}/ins_rm_with_target.sh"
  @ONLY
)

kedr_test_install(PROGRAMS "ins_rm.sh" "ins_rm_with_target.sh")

kedr_test_add_script_shared (core.basics.01 
    ins_rm.sh
)

# Here, kedr_sample_target.ko module (from the examples) will be used as 
# a target.

# Check common usage scenarios
kedr_test_add_script_shared (core.basics.02 
    ins_rm_with_target.sh 0
)

# Check the case when the target is loaded before the KEDR
kedr_test_add_script_shared (core.basics.03
    ins_rm_with_target.sh 1
)
