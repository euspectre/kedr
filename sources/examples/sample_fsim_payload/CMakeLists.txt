set(module_name "kedr_sample_fsim_payload")
set(install_dir "${KEDR_EXAMPLE_PATH}/sample_fsim_payload")

set(KEDR_INCLUDE_DIR "${KEDR_INSTALL_INCLUDE_DIR}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Kbuild.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Kbuild_installed"
    @ONLY
)

set(kedr_symbols "${KEDR_INSTALL_PREFIX_KSYMVERS}/kedr.symvers")
set(fault_simulation_symbols "${KEDR_INSTALL_PREFIX_KSYMVERS}/kedr_fault_simulation.symvers")
set(kedr_gen_tool "${KEDR_INSTALL_PREFIX_EXEC_AUX}/kedr_gen")
set(kedr_gen_templates_dir "${KEDR_INSTALL_PREFIX_TEMPLATES}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/makefile.in"
    "${CMAKE_CURRENT_BINARY_DIR}/makefile_installed"
    @ONLY
)

install(FILES   "${CMAKE_CURRENT_SOURCE_DIR}/payload.c"
                "${CMAKE_CURRENT_SOURCE_DIR}/functions.data"
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

kedr_test_add_subdirectory(tests)