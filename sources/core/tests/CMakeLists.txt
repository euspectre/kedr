########################################################################
# Add the tests here and set their properties if necessary.
#
# [Naming conventions]
# Name of each test should be unique and should have the following format:
#   <namespace>[.group].<numeric_id>
# For global tests, <namespace> is the name of the package ("kedr").
# For local tests, <namespace> is the name of the respective component of
#   KEDR (like "controller", etc.).
# The numeric ID should contain at least two digits. It is not required for
# the IDs to go in any particular order.
# 
# A name of a test must not be a substring of a name of some other test.
# For example, using "aaa.01" and "aaa.011" together is not allowed but
# it is acceptable to use "aaa.001" and "aaa.011" together.
########################################################################


## Should be revisited.

# Paths to and the name of the KEDR core module
set (CORE_DIR "${CMAKE_BINARY_DIR}/core")
set (CORE_NAME "kedr")

set (TARGET_NAME "kedr_sample_target")
set (TARGET_LOAD_CMD 
    "sh ${CMAKE_BINARY_DIR}/examples/sample_target/tests/kedr_sample_target load")
set (TARGET_UNLOAD_CMD 
    "sh ${CMAKE_BINARY_DIR}/examples/sample_target/tests/kedr_sample_target unload")

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

kedr_test_add_script (core.basics.01 
    ins_rm.sh
)

# Here, kedr_sample_target.ko module (from the examples) will be used as 
# a target.

# Check common usage scenarios
kedr_test_add_script (core.basics.02 
    ins_rm_with_target.sh 0
)

# Check the case when the target is loaded before the KEDR
kedr_test_add_script (core.basics.03
    ins_rm_with_target.sh 1
)

# The stuff necessary for testing
add_subdirectory (simple_payload)

# More tests
add_subdirectory (call_interception)
