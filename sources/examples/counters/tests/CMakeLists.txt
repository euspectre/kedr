set (KEDR_TOOLS_DIR "${CMAKE_BINARY_DIR}/tools")

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

kedr_test_add_script (examples.counters.01 
    test_counters.sh
)

