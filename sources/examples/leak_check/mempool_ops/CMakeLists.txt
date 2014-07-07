set(module_name "leak_check_mempool_ops")
set(example_install_dir "${KEDR_EXAMPLE_PATH}/leak_check/mempool_ops")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/payload.data.in"
	"${CMAKE_CURRENT_BINARY_DIR}/payload.data"
	@ONLY)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Kbuild.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Kbuild_installed"
    @ONLY
)

set(kedr_symbols "${KEDR_INSTALL_PREFIX_KSYMVERS}/kedr.symvers")
set(kedr_leak_check_symbols "${KEDR_INSTALL_PREFIX_KSYMVERS}/kedr_leak_check.symvers")
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

install(FILES   "${CMAKE_CURRENT_SOURCE_DIR}/Readme.txt"
        DESTINATION "${example_install_dir}"
)
