configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test_fault_simulation.sh.in"
	"${CMAKE_CURRENT_BINARY_DIR}/test_fault_simulation.sh"
	@ONLY
)

kedr_test_add_script("sample_fsim_payload.01" "test_fault_simulation.sh")