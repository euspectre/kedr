set(module_name "indicator_fsim_kstrdup")
set(example_install_dir "${KEDR_EXAMPLE_PATH}/custom_indicator_fsim")

kedr_load_install_prefixes()
set(KEDR_INCLUDE_DIR "${KEDR_INSTALL_INCLUDE_DIR}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Kbuild.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Kbuild_installed"
    @ONLY
)

set(kedr_symbols "${KEDR_INSTALL_PREFIX_KSYMVERS}/kedr.symvers")
set(fault_simulation_symbols "${KEDR_INSTALL_PREFIX_KSYMVERS}/kedr_fault_simulation.symvers")
set(kedr_gen_tool "${KEDR_INSTALL_PREFIX_EXEC_AUX}/kedr_gen")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/makefile.in"
    "${CMAKE_CURRENT_BINARY_DIR}/makefile_installed"
    @ONLY
)

# Additional sources from which indicator module will compile
set(additional_sources
	"${CMAKE_SOURCE_DIR}/control_file/control_file.c"
	"${CMAKE_SOURCE_DIR}/calculator/calculator.c")
	
# Same sources but after copiing them into current directory.
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


# Copy additional sources into this directory
#rule_copy_file("control_file.c"
#	"${CMAKE_SOURCE_DIR}/control_file/control_file.c")
#rule_copy_file("calculator.c"
#	"${CMAKE_SOURCE_DIR}/calculator/calculator.c")
#rule_copy_file("stack_trace.c"
#	"${CMAKE_SOURCE_DIR}/util/stack_trace/stack_trace.c")


#For testing
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

rule_copy_source(indicator.data)

set(kedr_symbols "${CMAKE_BINARY_DIR}/core/Module.symvers")
set(fault_simulation_symbols "${CMAKE_BINARY_DIR}/fault_simulation/Module.symvers")
set(kedr_gen_tool "${KEDR_GEN_INSTALL_PREFIX}/kedr_gen")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/makefile.in"
    "${CMAKE_CURRENT_BINARY_DIR}/makefile_test"
    @ONLY
)

kedr_test_add_target("custom_fsim_indicator")

install(FILES   "${CMAKE_CURRENT_SOURCE_DIR}/indicator.data"
				${additional_sources}
        DESTINATION "${example_install_dir}"
)

install(FILES   "${CMAKE_CURRENT_BINARY_DIR}/makefile_installed"
        DESTINATION "${example_install_dir}"
        RENAME makefile
)

install(FILES   "${CMAKE_CURRENT_BINARY_DIR}/Kbuild_installed"
        DESTINATION "${example_install_dir}"
        RENAME Kbuild
)
