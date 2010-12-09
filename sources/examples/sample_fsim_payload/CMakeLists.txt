set(module_name "kedr_sample_fsim_payload")
set(install_dir "${KEDR_EXAMPLE_PATH}/sample_fsim_payload")

set(KEDR_INCLUDE_DIR "${KEDR_INSTALL_INCLUDE_DIR}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Kbuild.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Kbuild_installed"
    @ONLY
)

set(kedr_base_symbols "${KEDR_INSTALL_PREFIX_KSYMVERS}/kedr_base.symvers")
set(fault_simulation_symbols "${KEDR_INSTALL_PREFIX_KSYMVERS}/kedr_fault_simulation.symvers")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/makefile.in"
    "${CMAKE_CURRENT_BINARY_DIR}/makefile_installed"
    @ONLY
)

#For testing
add_custom_target("sample_fsim_payload"
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${module_name}.ko"
                )

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${module_name}.ko"
        COMMAND $(MAKE) -f makefile_test
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/sample_fsim_payload.c"
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
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/makefile.in"
    "${CMAKE_CURRENT_BINARY_DIR}/makefile_test"
    @ONLY
)

rule_copy_source("sample_fsim_payload.c")

kedr_test_add_target("sample_fsim_payload")

#add_subdirectory(tests)

install(FILES   "${CMAKE_CURRENT_SOURCE_DIR}/sample_fsim_payload.c"
        DESTINATION ${install_dir}
)

install(FILES   "${CMAKE_CURRENT_BINARY_DIR}/makefile_installed"
        DESTINATION ${install_dir}
        RENAME makefile
)

install(FILES   "${CMAKE_CURRENT_BINARY_DIR}/Kbuild_installed"
        DESTINATION ${install_dir}
        RENAME Kbuild
)