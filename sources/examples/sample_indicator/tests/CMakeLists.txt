add_custom_target("sample_indicator"
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${module_name}.ko"
                )

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${module_name}.ko"
        COMMAND $(MAKE) -f makefile_test
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/sample_indicator.c"
                "${CMAKE_CURRENT_BINARY_DIR}/makefile_test"
                "${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
                )

set(KEDR_INCLUDE_DIR "${KEDR_TEST_INCLUDE_DIR}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../Kbuild.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
    @ONLY
)

set(fault_simulation_symbols "${CMAKE_BINARY_DIR}/fault_simulation/Module.symvers")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../makefile.in"
    "${CMAKE_CURRENT_BINARY_DIR}/makefile_test"
    @ONLY
)

rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/sample_indicator.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/../sample_indicator.c")

kedr_test_add_target("sample_indicator")

add_subdirectory(module)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
	"${CMAKE_CURRENT_BINARY_DIR}/test.sh"
	@ONLY)

kedr_test_add_script("sample_indicator.01" "test.sh")