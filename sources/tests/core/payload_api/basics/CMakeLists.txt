# The following subdirectories contain the stuff necessary for the testing
add_subdirectory (payload)
add_subdirectory (target_normal)
add_subdirectory (target_init_fail)
add_subdirectory (target-with-dashes)

# The tests for target load / unload notifications mechanism provided by 
# KEDR core for payload modules.

# '@ONLY' is essential when doing substitutions in the shell scripts. 
# Without it, CMake would replace "${...}" too, which is usually not what 
# you want.
configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
  "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
  @ONLY
)

configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/test_set_target.sh.in"
  "${CMAKE_CURRENT_BINARY_DIR}/test_set_target.sh"
  @ONLY
)

configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/test_change_target.sh.in"
  "${CMAKE_CURRENT_BINARY_DIR}/test_change_target.sh"
  @ONLY
)

kedr_test_install(PROGRAMS "test.sh" "test_set_target.sh" "test_change_target.sh")

# test.sh takes three arguments: 
#   <target_dir>, <set_load_fn> and <set_unload_fn>. 
# <target_dir> is the name of the directory containing target module.
# Both set_* arguments can be zero or non-zero. These arguments specify 
# whether to instruct the payload module to set callbacks for target module 
# load and unload events.
# !=0 means "set", 0 - "do not set".
kedr_test_add_script_shared (payload_api.notify.01 
    test.sh "target_normal" 0 1
)

kedr_test_add_script_shared (payload_api.notify.02 
    test.sh "target_normal" 1 0
)

kedr_test_add_script_shared (payload_api.notify.03 
    test.sh "target_normal" 1 1
)

kedr_test_add_script_shared (payload_api.notify.04 
    test.sh "target_init_fail" 1 1
)

kedr_test_add_script_shared (payload_api.notify.05 
    test.sh "target_init_fail" 1 0
)

kedr_test_add_script_shared (payload_api.notify.06 
    test.sh "target_init_fail" 0 1
)

kedr_test_add_script_shared (payload_api.notify.07 
    test.sh "target-with-dashes" 1 1
)

kedr_test_add_script_shared (payload_api.set_target.01 
    test_set_target.sh "target-with-dashes"
)

kedr_test_add_script_shared (payload_api.change_target.01 
    test_change_target.sh "target_normal" "target-with-dashes"
)

