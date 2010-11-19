set(module_name "kedr_sample_indicator")
set(install_dir "${KEDR_EXAMPLE_PATH}/sample_indicator")

set(KEDR_INCLUDE_DIR "${KEDR_INSTALL_INCLUDE_DIR}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Kbuild.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Kbuild_installed"
    @ONLY
)

set(fault_simulation_symbols "${KEDR_INSTALL_PREFIX_KSYMVERS}/kedr_fault_simulation.symvers")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/makefile.in"
    "${CMAKE_CURRENT_BINARY_DIR}/makefile_installed"
    @ONLY
)

#For testing
add_custom_target("sample_indicator"
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${module_name}.ko"
                )

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${module_name}.ko"
        COMMAND make -f makefile_test
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/sample_indicator.c"
                "${CMAKE_CURRENT_BINARY_DIR}/makefile_test"
                "${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
                )

set(KEDR_INCLUDE_DIR "${KEDR_TEST_INCLUDE_DIR}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Kbuild.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
    @ONLY
)

set(fault_simulation_symbols "${CMAKE_BINARY_DIR}/fault_simulation/Module.symvers")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/makefile.in"
    "${CMAKE_CURRENT_BINARY_DIR}/makefile_test"
    @ONLY
)

rule_copy_source("sample_indicator.c")

kedr_test_add_target("sample_indicator")

add_subdirectory(tests)

install(FILES   "${CMAKE_CURRENT_SOURCE_DIR}/sample_indicator.c"
        DESTINATION "${install_dir}$"
)

install(FILES   "${CMAKE_CURRENT_BINARY_DIR}/makefile_installed"
        DESTINATION "${install_dir}$"
        RENAME makefile
)

install(FILES   "${CMAKE_CURRENT_BINARY_DIR}/Kbuild_installed"
        DESTINATION "${install_dir}$"
        RENAME Kbuild
)