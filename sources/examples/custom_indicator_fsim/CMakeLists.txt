set(module_name "indicator_fsim_kstrdup")
set(example_install_dir "${KEDR_EXAMPLE_PATH}/custom_indicator_fsim")

if(KEDR_ENABLE_CALLER_ADDRESS)
	set(KEDR_ENABLE_CALLER_ADDRESS_DEF "yes")
endif(KEDR_ENABLE_CALLER_ADDRESS)

set(KEDR_INCLUDE_DIR "${KEDR_INSTALL_INCLUDE_DIR}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Kbuild.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Kbuild_installed"
    @ONLY
)

set(kedr_base_symbols "${KEDR_INSTALL_PREFIX_KSYMVERS}/kedr_base.symvers")
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
	
if(KEDR_ENABLE_CALLER_ADDRESS)
	list(APPEND additional_sources
		"${CMAKE_SOURCE_DIR}/util/stack_trace/stack_trace.c")
endif(KEDR_ENABLE_CALLER_ADDRESS)

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


# Copy templates files into build dir
set(templates_root_dir_src "${CMAKE_SOURCE_DIR}/fault_indicators/templates")
set(templates_root_dir_dst "${CMAKE_CURRENT_BINARY_DIR}/templates")

set(templates_dirs
	"indicator.c/block" "indicator.c/document"
	"indicator_internal.data/block" "indicator_internal.data/document")
set(templates_dirs_src)
set(templates_dirs_dst)
set(templates_files_glob_expr)

foreach(template_dir ${templates_dirs})
	list(APPEND templates_dirs_src "${templates_root_dir_src}/${template_dir}")
	list(APPEND templates_dirs_dst "${templates_root_dir_dst}/${template_dir}")
	list(APPEND templates_files_glob_expr "${templates_root_dir_src}/${template_dir}/*.tpl")
endforeach(template_dir ${templates_dirs})
file(GLOB custom_indicator_templates_files RELATIVE "${templates_root_dir_src}" ${templates_files_glob_expr})

set(custom_indicator_templates_files_dst)
set(custom_indicator_templates_files_src)
foreach(template_file ${custom_indicator_templates_files})
	list(APPEND custom_indicator_templates_files_src "${templates_root_dir_src}/${template_file}")	
	list(APPEND custom_indicator_templates_files_dst "${templates_root_dir_dst}/${template_file}")
endforeach(template_file ${custom_indicator_templates_files})


add_custom_target("custom_fsim_indicator_templates"
        DEPENDS ${custom_indicator_templates_files_dst}
                )

# Without dependencies - for simplification
file(MAKE_DIRECTORY ${templates_dirs_dst})

foreach(template_file ${custom_indicator_templates_files})
	add_custom_command(OUTPUT "${templates_root_dir_dst}/${template_file}"
		COMMAND cp ${templates_root_dir_src}/${template_file} ${templates_root_dir_dst}/${template_file}
		DEPENDS "${templates_root_dir_src}/${template_file}")
endforeach(template_file ${custom_indicator_templates_files})

#For testing
add_custom_target("custom_fsim_indicator"
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${module_name}.ko"
                )

# Copy template files into build directory only for test building process
add_dependencies("custom_fsim_indicator" "custom_fsim_indicator_templates")



add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${module_name}.ko"
        COMMAND $(MAKE) -f makefile_test
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/indicator.data"
				${additional_sources_test}
                "${CMAKE_CURRENT_BINARY_DIR}/makefile_test"
                "${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
                )

set(KEDR_INCLUDE_DIR "${KEDR_TEST_INCLUDE_DIR}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Kbuild.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
    @ONLY
)

rule_copy_source(indicator.data)

set(kedr_base_symbols "${CMAKE_BINARY_DIR}/base/Module.symvers")
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

install(DIRECTORY  "${templates_root_dir_src}"
        DESTINATION "${example_install_dir}"
)
