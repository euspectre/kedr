# The path where the examples will be installed
set (KEDR_EXAMPLE_PATH "${KEDR_INSTALL_PREFIX_EXAMPLES}")

# Base directories for header files provided with KEDR. 
# The goal is to allow the examples use #include directives like this:
#    #include <kedr/base/common.h>
# As the directives are different when KEDR is already installed and during
# the testing ('make check'), two paths are provided.

set (KEDR_INSTALL_INCLUDE_DIR "${CMAKE_INSTALL_PREFIX}/include")
# See comments in include/CMakeLists.txt about headers in examples.
set (KEDR_TEST_INCLUDE_DIR "${CMAKE_BINARY_DIR}/include/install")
#######################################################################

add_subdirectory(sample_target)
add_subdirectory(sample_indicator)
add_subdirectory(sample_fsim_payload)
add_subdirectory(counters)
if(KEDR_CALLM_PAYLOADS)
    add_subdirectory(custom_payload_callm)
endif(KEDR_CALLM_PAYLOADS)
add_subdirectory(custom_payload_fsim)
add_subdirectory(custom_indicator_fsim)

kedr_test_add_subdirectory(tests)
