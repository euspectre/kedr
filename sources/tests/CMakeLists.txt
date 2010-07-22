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

# Path to the controller module and its name.
set (CONTROLLER_DIR "${CMAKE_BINARY_DIR}/controller")
set (CONTROLLER_NAME "kedr_controller")

# '@ONLY' is essential when doing substitutions in the shell scripts. 
# Without it, CMake would replace "${...}" too, which is usually not what 
# you want.
configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/sample.sh.in"
  "${CMAKE_CURRENT_BINARY_DIR}/sample.sh"
  @ONLY
)

kedr_test_add_script (kedr.sample.01 sample.sh)

# The tests for API provided by the KEDR core to the payload modules.
add_subdirectory (payload_api)
