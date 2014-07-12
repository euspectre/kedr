set(module_name "kedr_sample_fsim_payload")
set(example_install_dir "${KEDR_EXAMPLE_PATH}/sample_fsim_payload")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Kbuild.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
    @ONLY
)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/makefile.in"
    "${CMAKE_CURRENT_BINARY_DIR}/example_makefile"
    @ONLY
)

example_add(example_sample_fsim_payload
    "payload.c"
    "functions.data"
    "makefile" SOURCE "example_makefile"
    "Kbuild"
)

example_install(TARGETS example_sample_fsim_payload
    EXPORT test_example_sample_fsim_payload
    DESTINATION ${example_install_dir}
)
