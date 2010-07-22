# Paths to and the names of the base and controller modules
set (CONTROLLER_DIR "${CMAKE_BINARY_DIR}/controller")
set (CONTROLLER_NAME "kedr_controller")

set (BASE_DIR "${CMAKE_BINARY_DIR}/base")
set (BASE_NAME "kedr_base")

# '@ONLY' is essential when doing substitutions in the shell scripts. 
# Without it, CMake would replace "${...}" too, which is usually not what 
# you want.
configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
  "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
  @ONLY
)

# test.sh takes two arguments: <in_init> and <in_atomic>. Both can be zero
# or non-zero. 
# <in_init> specifies whether the call to the target function should be made
# at the initialization stage of the target module (!= 0) or from the module 
# core (== 0)
# <in_atomic> specifies whether that call should be made from atomic context
# (!= 0) or not (== 0).

kedr_test_add_script (payload_api.in_init.01 
    test.sh 0 0
)

kedr_test_add_script (payload_api.in_init.02 
    test.sh 0 1
)

kedr_test_add_script (payload_api.in_init.03 
    test.sh 1 0
)

kedr_test_add_script (payload_api.in_init.04 
    test.sh 1 1
)

# The following subdirectories contain the stuff necessary for the testing
add_subdirectory (payload)
add_subdirectory (target)
