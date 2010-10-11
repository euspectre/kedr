set(module_name "kedr_sample_indicator_kmalloc")

kedr_load_install_prefixes()
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Kbuild.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Kbuild_installed"
    @ONLY
)

set(fault_simulation_symbols "${KEDR_INSTALL_PREFIX_KSYMVERS}/kedr_fault_simulation.symvers")
set(kedr_base_symbols "${KEDR_INSTALL_PREFIX_KSYMVERS}/kedr_base.symvers")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/makefile.in"
    "${CMAKE_CURRENT_BINARY_DIR}/makefile_installed"
    @ONLY
)

#For testing
add_custom_target("indicator_kmalloc"
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${module_name}.ko"
                )

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${module_name}.ko"
        COMMAND make -f makefile_test
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/indicator_kmalloc.c"
                "${CMAKE_CURRENT_BINARY_DIR}/calculator.h"
                "${CMAKE_CURRENT_BINARY_DIR}/calculator.c"
                "${CMAKE_CURRENT_BINARY_DIR}/makefile_installed"
                "${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
                )

kedr_load_test_prefixes()
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Kbuild.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
    @ONLY
)

set(fault_simulation_symbols "${CMAKE_BINARY_DIR}/fault_simulation/Module.symvers")
set(kedr_base_symbols "${CMAKE_BINARY_DIR}/base/Module.symvers")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/makefile.in"
    "${CMAKE_CURRENT_BINARY_DIR}/makefile_test"
    @ONLY
)

rule_copy_source("indicator_kmalloc.c")
rule_copy_file("calculator.h" "${CMAKE_SOURCE_DIR}/include/kedr/fault_simulation/calculator.h")

#Configure calculator implementation for use by kedr-user
set(CALCULATOR_HEADER "calculator.h")
configure_file("${CMAKE_SOURCE_DIR}/fault_simulation/calculator/calculator.c.in"
	"${CMAKE_CURRENT_BINARY_DIR}/calculator.c"
	@ONLY)

kedr_test_add_target("indicator_kmalloc")


#add_subdirectory(tests)

install(FILES   "${CMAKE_CURRENT_BINARY_DIR}/indicator_kmalloc.c"
				"${CMAKE_CURRENT_BINARY_DIR}/calculator.c"
				"${CMAKE_CURRENT_BINARY_DIR}/calculator.h"
        DESTINATION "${KEDR_EXAMPLE_PATH}/indicator_kmalloc"
)

install(FILES   "${CMAKE_CURRENT_BINARY_DIR}/makefile_installed"
        DESTINATION "${KEDR_EXAMPLE_PATH}/indicator_kmalloc"
        RENAME makefile
)

install(FILES   "${CMAKE_CURRENT_BINARY_DIR}/Kbuild_installed"
        DESTINATION "${KEDR_EXAMPLE_PATH}/indicator_kmalloc"
        RENAME Kbuild
)