set(KEDR_TEST_LEAKER_MODULE "kedr_test_leaker")
set(KEDR_TEST_CLEANER_MODULE "kedr_test_cleaner")

# Where to find the common test-related scripts, etc.
set(KEDR_TOOLS_DIR "${CMAKE_BINARY_DIR}/tools")

# '@ONLY' is essential when doing substitutions in the shell scripts. 
# Without it, CMake would replace "${...}" too, which is usually not what 
# you want.
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
  "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
  @ONLY
)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/leak_check_test.conf.in"
  "${CMAKE_CURRENT_BINARY_DIR}/leak_check_test.conf"
  @ONLY
)

kedr_test_add_script(leak_check.01 
    test.sh
)

add_subdirectory(leaker_module)
add_subdirectory(cleaner_module)
