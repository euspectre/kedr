set(KEDR_TEST_DIR "${KEDR_TEST_PREFIX_TEMP_SESSION}/examples/counters")

example_create_build_script("build_example.sh" counters)
kedr_test_install(PROGRAMS "build_example.sh")

# Directory where example will be built for testing purposes.
set(EXAMPLE_TEST_LOCATION "build")

# '@ONLY' is essential when doing substitutions in the shell scripts. 
# Without it, CMake would replace "${...}" too, which is usually not what 
# you want.
configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/test_counters.sh.in"
  "${CMAKE_CURRENT_BINARY_DIR}/test_counters.sh"
  @ONLY
)

configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/counters.conf.in"
  "${CMAKE_CURRENT_BINARY_DIR}/counters.conf"
  @ONLY
)

kedr_test_install(FILES "counters.conf")

kedr_test_add_script (examples.counters.01 
    test_counters.sh
)

