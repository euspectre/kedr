set(KMODULE_NAME "simple_target")

kbuild_include_directories ("${CMAKE_CURRENT_SOURCE_DIR}")
kbuild_add_module(${KMODULE_NAME} 
    "simple_target.c"
    "simple_target.h"
)

configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/target_load.sh.in"
  "${CMAKE_CURRENT_BINARY_DIR}/target_load.sh"
  @ONLY
)

configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/target_unload.sh.in"
  "${CMAKE_CURRENT_BINARY_DIR}/target_unload.sh"
  @ONLY
)

kedr_test_add_target (${KMODULE_NAME})
