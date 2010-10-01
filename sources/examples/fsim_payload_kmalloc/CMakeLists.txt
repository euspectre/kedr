set(module_name "kedr_fsim_payload_kmalloc")

kedr_load_install_prefixes()
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
add_custom_target("fsim_payload_kmalloc"
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${module_name}.ko"
                )

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${module_name}.ko"
        COMMAND make -f makefile_test
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/fsim_payload_kmalloc.c"
                "${CMAKE_CURRENT_BINARY_DIR}/makefile_installed"
                "${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
                )

kedr_load_test_prefixes()
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

rule_copy_source("fsim_payload_kmalloc.c")

kedr_test_add_target("fsim_payload_kmalloc")

#add_subdirectory(tests)

install(FILES   "${CMAKE_CURRENT_SOURCE_DIR}/fsim_payload_kmalloc.c"
        DESTINATION "${KEDR_EXAMPLE_PATH}/fsim_payload_kmalloc"
)

install(FILES   "${CMAKE_CURRENT_SOURCE_DIR}/makefile_installed"
        DESTINATION "${KEDR_EXAMPLE_PATH}/fsim_payload_kmalloc"
        RENAME makefile
)

install(FILES   "${CMAKE_CURRENT_SOURCE_DIR}/Kbuild_installed"
        DESTINATION "${KEDR_EXAMPLE_PATH}/fsim_payload_kmalloc"
        RENAME Kbuild
)