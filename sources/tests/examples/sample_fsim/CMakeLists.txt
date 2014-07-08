# Test which combine several examples together
set(KEDR_TEST_DIR "${KEDR_TEST_PREFIX_TEMP_SESSION}/examples/sample_fsim")

# We need to exclusively build every example which we use, whenever
# this example is built in another test.
#
# This is a price for testing install configuration instead of build one.
example_create_build_script(test_example_sample_indicator "build_indicator.sh")
kedr_test_install(PROGRAMS "build_indicator.sh")

set(sample_indicator_test_location "build_indicator")

example_create_build_script(test_example_sample_fsim_payload "build_payload.sh")
kedr_test_install(PROGRAMS "build_payload.sh")

set(sample_fsim_payload_test_location "build_payload")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test_fault_simulation.sh.in"
	"${CMAKE_CURRENT_BINARY_DIR}/test_fault_simulation.sh"
    @ONLY
)

kedr_test_add_script("examples.sample_fsim.01" "test_fault_simulation.sh")

