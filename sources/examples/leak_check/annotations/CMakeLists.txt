set(module_name "leak_check_annotations")
set(example_install_dir "${KEDR_EXAMPLE_PATH}/leak_check/annotations")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Kbuild.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
    @ONLY
)

set(kedr_leak_check_symbols "${KEDR_INSTALL_PREFIX_KSYMVERS}/kedr_leak_check.symvers")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/makefile.in"
    "${CMAKE_CURRENT_BINARY_DIR}/example_makefile"
    @ONLY
)

example_add(example_leak_check_annotations
    "makefile" SOURCE "example_makefile"
    "Kbuild"
    "module.c"
    "cfake.h"
    "Readme.txt"
)

example_install(TARGETS example_leak_check_annotations
    EXPORT test_example_leak_check_annotations
    DESTINATION ${example_install_dir}
)
