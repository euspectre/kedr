# '@ONLY' is essential when doing substitutions in the shell scripts. 
# Without it, CMake would replace "${...}" too, which is usually not what 
# you want.
configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
  "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
  @ONLY
)

kedr_test_install(PROGRAMS "test.sh")

# test.sh takes two arguments: <in_init> and <in_atomic>. Both can be zero
# or non-zero. 
# <in_init> specifies whether the call to the target function should be made
# at the initialization stage of the target module (!= 0) or from the module 
# core (== 0)
# <in_atomic> specifies whether that call should be made from atomic context
# (!= 0) or not (== 0).

kedr_test_add_script_shared (payload_api.in_init.01 
    test.sh 0 0
)

kedr_test_add_script_shared (payload_api.in_init.02 
    test.sh 0 1
)

kedr_test_add_script_shared (payload_api.in_init.03 
    test.sh 1 0
)

kedr_test_add_script_shared (payload_api.in_init.04 
    test.sh 1 1
)

# The following subdirectories contain the stuff necessary for the testing
add_subdirectory (payload)
add_subdirectory (target)
