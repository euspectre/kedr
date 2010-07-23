# Paths to and the names of the base and controller modules
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

# test.sh takes a single argument, <scenario_number>.
#
# <scenario_number>:
#   0 - register + unregister, then again register + unregister (with 
# correct data each time)
#   1 - attempt to register the same payload twice (the second call must 
# fail)
#   2 - register + attempt to unregister some other payload + unregister
# correctly (must not crash, no explicit checks)
#
# Loading of the payload module must succeed in each scenario. The result of 
# the test is output via "test_passed" parameter (see sysfs).

kedr_test_add_script (payload_api.register.01 
    test.sh 0
)

kedr_test_add_script (payload_api.register.02 
    test.sh 1
)

kedr_test_add_script (payload_api.register.03 
    test.sh 2
)

add_subdirectory (payload)
