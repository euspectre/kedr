set(module_name "indicator_fsim_kstrdup")
set(example_install_dir "${KEDR_EXAMPLE_PATH}/custom_indicator_fsim")

if(KEDR_ENABLE_CALLER_ADDRESS)
	set(SET_KEDR_ENABLE_CALLER_ADDRESS "KEDR_ENABLE_CALLER_ADDRESS := yes")
else(KEDR_ENABLE_CALLER_ADDRESS)
	set(SET_KEDR_ENABLE_CALLER_ADDRESS "KEDR_ENABLE_CALLER_ADDRESS := ")
endif(KEDR_ENABLE_CALLER_ADDRESS)

kedr_load_install_prefixes()
set(KEDR_INCLUDE_DIR "${KEDR_INSTALL_INCLUDE_DIR}")
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

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/config.h"
)

# Additional sources from which indicator module will compile
set(additional_sources
	"${CMAKE_SOURCE_DIR}/control_file/control_file.c"
	"${CMAKE_SOURCE_DIR}/calculator/calculator.c")
	

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

install(FILES   "${CMAKE_CURRENT_BINARY_DIR}/config.h"
        DESTINATION "${example_install_dir}"
)
kedr_test_add_subdirectory(tests)