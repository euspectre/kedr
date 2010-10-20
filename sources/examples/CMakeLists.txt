# The path where the examples will be installed
set (KEDR_EXAMPLE_PATH "${KEDR_INSTALL_PREFIX_EXAMPLES}")

# Base directories for header files provided with KEDR. 
# The goal is to allow the examples use #include directives like this:
#    #include <kedr/base/common.h>
# As the directives are different when KEDR is already installed and during
# the testing ('make check'), two paths are provided.

set (KEDR_INSTALL_INCLUDE_DIR "${CMAKE_INSTALL_PREFIX}/include")
set (KEDR_TEST_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
#######################################################################

add_subdirectory(sample_target)
add_subdirectory(sample_target_2)
add_subdirectory(indicator_kmalloc)
add_subdirectory(fsim_payload_kmalloc)
add_subdirectory(counters)
add_subdirectory(custom_payload_callm)
add_subdirectory(custom_payload_fsim)
add_subdirectory(custom_indicator_fsim)

add_subdirectory(tests)
