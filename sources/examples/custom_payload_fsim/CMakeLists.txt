set(module_name "payload_fsim_kstrdup")
set(example_install_dir "${KEDR_EXAMPLE_PATH}/custom_payload_fsim")

if(KEDR_ENABLE_CALLER_ADDRESS)
	set(SET_KEDR_ENABLE_CALLER_ADDRESS "KEDR_ENABLE_CALLER_ADDRESS := yes")
else(KEDR_ENABLE_CALLER_ADDRESS)
	set(SET_KEDR_ENABLE_CALLER_ADDRESS "KEDR_ENABLE_CALLER_ADDRESS := ")
endif(KEDR_ENABLE_CALLER_ADDRESS)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/payload.data.in"
	"${CMAKE_CURRENT_BINARY_DIR}/payload.data"
	@ONLY)

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

install(FILES   "${CMAKE_CURRENT_BINARY_DIR}/payload.data"
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
