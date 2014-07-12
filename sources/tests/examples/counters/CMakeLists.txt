set(KEDR_TEST_DIR "${KEDR_TEST_PREFIX_TEMP_SESSION}/examples/counters")

if(NOT USER_PART)
    return()
endif(NOT USER_PART)

example_create_build_script(test_example_counters "build_example.sh")
kedr_test_install(PROGRAMS "build_example.sh")

# Directory where example will be built for testing purposes.
#
# Actually, additionally suffix ${KERNEL} will be added when build example,
# but resulted kernel module will be copied into kernel-independent
# This copy is needed for loading module by kernel-independent config file.
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

