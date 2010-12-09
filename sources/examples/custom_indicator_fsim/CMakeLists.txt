set(module_name "indicator_fsim_kstrdup")
set(example_install_dir "${KEDR_EXAMPLE_PATH}/custom_indicator_fsim")


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

# Copy additional headers into this directory
rule_copy_file("control_file.h"
	"${CMAKE_SOURCE_DIR}/include/kedr/control_file/control_file.h")
rule_copy_file("calculator.h"
	"${CMAKE_SOURCE_DIR}/include/kedr/fault_simulation/calculator.h")

# Configure additional sources into this directory
set(CONTROL_FILE_HEADER "control_file.h")
configure_file("${CMAKE_SOURCE_DIR}/control_file/control_file.c.in"
	"${CMAKE_CURRENT_BINARY_DIR}/control_file.c")
set(CALCULATOR_HEADER "calculator.h")
configure_file("${CMAKE_SOURCE_DIR}/fault_simulation/calculator/calculator.c.in"
	"${CMAKE_CURRENT_BINARY_DIR}/calculator.c")


# Copy templates files into build dir
set(templates_root_dir_src "${CMAKE_SOURCE_DIR}/fault_indicators/templates")
set(templates_root_dir_dst "${CMAKE_CURRENT_BINARY_DIR}/templates")

set(templates_dirs "indicator.c/block" "indicator.c/document" "indicator_internal.data/block" "indicator_internal.data/document")

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
				"${CMAKE_CURRENT_BINARY_DIR}/calculator.c"
				"${CMAKE_CURRENT_BINARY_DIR}/calculator.h"
				"${CMAKE_CURRENT_BINARY_DIR}/control_file.c"
				"${CMAKE_CURRENT_BINARY_DIR}/control_file.h"
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
				"${CMAKE_CURRENT_BINARY_DIR}/calculator.c"
				"${CMAKE_SOURCE_DIR}/include/kedr/fault_simulation/calculator.h"
				"${CMAKE_CURRENT_BINARY_DIR}/control_file.c"
				"${CMAKE_SOURCE_DIR}/include/kedr/control_file/control_file.h"
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
