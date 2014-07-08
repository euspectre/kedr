set(module_name "indicator_fsim_kstrdup")

set(example_install_dir "${KEDR_EXAMPLE_PATH}/custom_indicator_fsim")

if(KEDR_ENABLE_CALLER_ADDRESS)
    set(SET_KEDR_ENABLE_CALLER_ADDRESS "KEDR_ENABLE_CALLER_ADDRESS := yes")
else(KEDR_ENABLE_CALLER_ADDRESS)
    set(SET_KEDR_ENABLE_CALLER_ADDRESS "KEDR_ENABLE_CALLER_ADDRESS := ")
endif(KEDR_ENABLE_CALLER_ADDRESS)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Kbuild.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
    @ONLY
)

set(kedr_symbols "${KEDR_INSTALL_PREFIX_KSYMVERS}/kedr.symvers")
set(fault_simulation_symbols "${KEDR_INSTALL_PREFIX_KSYMVERS}/kedr_fault_simulation.symvers")
set(kedr_gen_tool "${KEDR_INSTALL_PREFIX_EXEC_AUX}/kedr_gen")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/makefile.in"
    "${CMAKE_CURRENT_BINARY_DIR}/example_makefile"
    @ONLY
)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/config.h"
)

example_add(example_custom_indicator_fsim
    "indicator.data"
    "control_file.c" SOURCE "${CMAKE_SOURCE_DIR}/control_file/control_file.c"
    "calculator.c" SOURCE "${CMAKE_SOURCE_DIR}/calculator/calculator.c"
    "makefile" SOURCE "example_makefile"
    "Kbuild"
    "config.h"
)

example_install(TARGETS example_custom_indicator_fsim
    EXPORT test_example_custom_indicator_fsim
    DESTINATION "${example_install_dir}"
)