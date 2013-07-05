# Same as 'additional_sources' but after copiing them into current directory.
# (This is needed for testing indicator module compilation).
set(additional_sources_test)
foreach(source ${additional_sources})
	get_filename_component(source_name "${source}" NAME)
	list(APPEND additional_sources_test
		"${CMAKE_CURRENT_BINARY_DIR}/${source_name}")
	#And how to get this file from source tree
	rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/${source_name}"
		"${source}")
endforeach(source ${additional_sources})


add_custom_target("custom_fsim_indicator"
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${module_name}.ko"
                )

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${module_name}.ko"
        COMMAND $(MAKE) -f makefile_test
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/indicator.data"
				${additional_sources_test}
                "${CMAKE_CURRENT_BINARY_DIR}/makefile_test"
                "${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
                )

kedr_load_test_prefixes()
set(KEDR_INCLUDE_DIR "${KEDR_TEST_INCLUDE_DIR}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Kbuild.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
    @ONLY
)

rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/indicator.data"
    "${CMAKE_CURRENT_SOURCE_DIR}/../indicator.data")

set(kedr_symbols "${CMAKE_BINARY_DIR}/core/Module.symvers")
set(fault_simulation_symbols "${CMAKE_BINARY_DIR}/fault_simulation/Module.symvers")
set(kedr_gen_tool "${KEDR_GEN_INSTALL_PREFIX}/kedr_gen")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../makefile.in"
    "${CMAKE_CURRENT_BINARY_DIR}/makefile_test"
    @ONLY
)

kedr_test_add_target("custom_fsim_indicator")
