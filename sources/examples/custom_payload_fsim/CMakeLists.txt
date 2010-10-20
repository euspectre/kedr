set(module_name "payload_fsim_kstrdup")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/payload.data.in"
	"${CMAKE_CURRENT_BINARY_DIR}/payload.data"
	@ONLY)


set(KEDR_INCLUDE_DIR "${KEDR_INSTALL_INCLUDE_DIR}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Kbuild.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Kbuild_installed"
    @ONLY
)

set(kedr_base_symbols "${KEDR_INSTALL_PREFIX_KSYMVERS}/kedr_base.symvers")
set(fault_simulation_symbols "${KEDR_INSTALL_PREFIX_KSYMVERS}/kedr_fault_simulation.symvers")
set(kedr_gen_tool "${KEDR_GEN_TOOL}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/makefile.in"
    "${CMAKE_CURRENT_BINARY_DIR}/makefile_installed"
    @ONLY
)

# Copy templates files into build dir
set(templates_root_dir_src "${CMAKE_SOURCE_DIR}/payloads_fsim/templates")
set(templates_root_dir_dst "${CMAKE_CURRENT_BINARY_DIR}/templates")

set(templates_dirs "payload.c/block" "payload.c/document" "trace_payload.h/block" "trace_payload.h/document")

set(templates_dirs_src)
set(templates_dirs_dst)
set(templates_files_glob_expr)

foreach(template_dir ${templates_dirs})
	list(APPEND templates_dirs_src "${templates_root_dir_src}/${template_dir}")
	list(APPEND templates_dirs_dst "${templates_root_dir_dst}/${template_dir}")
	list(APPEND templates_files_glob_expr "${templates_root_dir_src}/${template_dir}/*.tpl")
endforeach(template_dir ${templates_dirs})
file(GLOB custom_payload_templates_files RELATIVE "${templates_root_dir_src}" ${templates_files_glob_expr})

set(custom_payload_templates_files_dst)
set(custom_payload_templates_files_src)
foreach(template_file ${custom_payload_templates_files})
	list(APPEND custom_payload_templates_files_src "${templates_root_dir_src}/${template_file}")	
	list(APPEND custom_payload_templates_files_dst "${templates_root_dir_dst}/${template_file}")
endforeach(template_file ${custom_payload_templates_files})


add_custom_target("custom_payload_fsim_templates"
        DEPENDS ${custom_payload_templates_files_dst}
                )

# Without dependencies - for simplification
file(MAKE_DIRECTORY ${templates_dirs_dst})

foreach(template_file ${custom_payload_templates_files})
	add_custom_command(OUTPUT "${templates_root_dir_dst}/${template_file}"
		COMMAND cp ${templates_root_dir_src}/${template_file} ${templates_root_dir_dst}/${template_file}
		DEPENDS "${templates_root_dir_src}/${template_file}")
endforeach(template_file ${custom_payload_templates_files})

#For testing
add_custom_target("custom_payload_fsim"
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${module_name}.ko"
                )

# Copy template files into build directory only for test building process
add_dependencies("custom_payload_fsim" "custom_payload_fsim_templates")

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${module_name}.ko"
        COMMAND make -f makefile_test
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/payload.data"
                "${CMAKE_CURRENT_BINARY_DIR}/makefile_test"
                "${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
                )

set(KEDR_INCLUDE_DIR "${KEDR_TEST_INCLUDE_DIR}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Kbuild.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
    @ONLY
)

set(kedr_base_symbols "${CMAKE_BINARY_DIR}/base/Module.symvers")
set(fault_simulation_symbols "${CMAKE_BINARY_DIR}/fault_simulation/Module.symvers")
set(kedr_gen_tool "${KEDR_GEN_INSTALL_PREFIX}/kedr_gen")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/makefile.in"
    "${CMAKE_CURRENT_BINARY_DIR}/makefile_test"
    @ONLY
)

kedr_test_add_target("custom_payload_fsim")

install(FILES   "${CMAKE_CURRENT_BINARY_DIR}/payload.data"
        DESTINATION "${KEDR_EXAMPLE_PATH}/custom_payload_fsim"
)

install(FILES   "${CMAKE_CURRENT_BINARY_DIR}/makefile_installed"
        DESTINATION "${KEDR_EXAMPLE_PATH}/custom_payload_fsim"
        RENAME makefile
)

install(FILES   "${CMAKE_CURRENT_BINARY_DIR}/Kbuild_installed"
        DESTINATION "${KEDR_EXAMPLE_PATH}/custom_payload_fsim"
        RENAME Kbuild
)

install(DIRECTORY  "${templates_root_dir_src}"
        DESTINATION "${KEDR_EXAMPLE_PATH}/custom_payload_fsim"
)
