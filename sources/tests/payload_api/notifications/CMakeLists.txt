# The tests for target load / unload notifications mechanism provided by 
# KEDR core for payload modules.

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

# test.sh takes three arguments: 
#   <target_dir>, <set_load_fn> and <set_unload_fn>. 
# <target_dir> is the name of the directory containing target module.
# Both set_* arguments can be zero or non-zero. These arguments specify 
# whether to instruct the payload module to set callbacks for target module 
# load and unload events.
# !=0 means "set", 0 - "do not set".
kedr_test_add_script (payload_api.notifications.01 
    test.sh "target_normal" 0 1
)

kedr_test_add_script (payload_api.notifications.02 
    test.sh "target_normal" 1 0
)

kedr_test_add_script (payload_api.notifications.03 
    test.sh "target_normal" 1 1
)

kedr_test_add_script (payload_api.notifications.04 
    test.sh "target_init_fail" 1 1
)

kedr_test_add_script (payload_api.notifications.05 
    test.sh "target_init_fail" 1 0
)

kedr_test_add_script (payload_api.notifications.06 
    test.sh "target_init_fail" 0 1
)

# The following subdirectories contain the stuff necessary for the testing
add_subdirectory (payload)
add_subdirectory (target_normal)
add_subdirectory (target_init_fail)
