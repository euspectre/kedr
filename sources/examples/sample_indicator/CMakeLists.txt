set(module_name "kedr_sample_indicator")
set(example_install_dir "${KEDR_EXAMPLE_PATH}/sample_indicator")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Kbuild.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
    @ONLY
)

set(fault_simulation_symbols "${KEDR_INSTALL_PREFIX_KSYMVERS}/kedr_fault_simulation.symvers")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/makefile.in"
    "${CMAKE_CURRENT_BINARY_DIR}/example_makefile"
    @ONLY
)

example_add(example_sample_indicator
    "sample_indicator.c"
    "makefile" SOURCE "example_makefile"
    "Kbuild"
)

example_install(TARGETS example_sample_indicator
    EXPORT test_example_sample_indicator
    DESTINATION "${example_install_dir}"
)
