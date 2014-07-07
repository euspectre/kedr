add_subdirectory(counters)
add_subdirectory(custom_indicator_fsim)

if(KEDR_CALLM_PAYLOADS)
	add_subdirectory(custom_payload_callm)
endif(KEDR_CALLM_PAYLOADS)

add_subdirectory(custom_payload_fsim)

if(KEDR_LEAK_CHECK)
	add_subdirectory(leak_check)
endif(KEDR_LEAK_CHECK)

add_subdirectory(sample_fsim_payload)
add_subdirectory(sample_indicator)

# Test which combine several examples together
kedr_load_test_prefixes()

set(KEDR_TEST_DIR "${KEDR_TEST_PREFIX_TEMP_SESSION}/examples/tests")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test_fault_simulation.sh.in"
	"${CMAKE_CURRENT_BINARY_DIR}/test_fault_simulation.sh"
	@ONLY
)

kedr_test_add_script("sample_fsim_payload.01" "test_fault_simulation.sh")

