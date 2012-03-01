set(module_name "leak_check_annotations")
set(example_install_dir "${KEDR_EXAMPLE_PATH}/leak_check/annotations")

kedr_load_install_prefixes()
set(KEDR_INCLUDE_DIR "${KEDR_INSTALL_INCLUDE_DIR}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Kbuild.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Kbuild_installed"
    @ONLY
)

set(kedr_leak_check_symbols "${KEDR_INSTALL_PREFIX_KSYMVERS}/kedr_leak_check.symvers")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/makefile.in"
    "${CMAKE_CURRENT_BINARY_DIR}/makefile_installed"
    @ONLY
)

install(FILES   "${CMAKE_CURRENT_BINARY_DIR}/makefile_installed"
        DESTINATION "${example_install_dir}"
        RENAME makefile
)

install(FILES   "${CMAKE_CURRENT_BINARY_DIR}/Kbuild_installed"
        DESTINATION "${example_install_dir}"
        RENAME Kbuild
)

install(FILES   "${CMAKE_CURRENT_SOURCE_DIR}/module.c"
        DESTINATION "${example_install_dir}"
)

install(FILES   "${CMAKE_CURRENT_SOURCE_DIR}/cfake.h"
        DESTINATION "${example_install_dir}"
)

install(FILES   "${CMAKE_CURRENT_SOURCE_DIR}/Readme.txt"
        DESTINATION "${example_install_dir}"
)

kedr_test_add_subdirectory(tests)
