set(module_name "kedr_counters")

kedr_load_install_prefixes()
set(KEDR_INCLUDE_DIR "${KEDR_INSTALL_INCLUDE_DIR}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Kbuild.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Kbuild_installed"
    @ONLY
)

set(kedr_base_symbols "${KEDR_INSTALL_PREFIX_KSYMVERS}/kedr_base.symvers")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/makefile.in"
    "${CMAKE_CURRENT_BINARY_DIR}/makefile_installed"
    @ONLY
)

#######################################################################
# For testing
add_custom_target("counters_module"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${module_name}.ko"
)

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${module_name}.ko"
    COMMAND $(MAKE) -f makefile_test
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/counters.c"
            "${CMAKE_CURRENT_BINARY_DIR}/counters.h"
            "${CMAKE_CURRENT_BINARY_DIR}/makefile_installed"
            "${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
)

kedr_load_test_prefixes()
set(KEDR_INCLUDE_DIR "${KEDR_TEST_INCLUDE_DIR}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Kbuild.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
    @ONLY
)

set(kedr_base_symbols "${CMAKE_BINARY_DIR}/base/Module.symvers")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/makefile.in"
    "${CMAKE_CURRENT_BINARY_DIR}/makefile_test"
    @ONLY
)

rule_copy_source("counters.c")
rule_copy_source("counters.h")

kedr_test_add_target("counters_module")

add_subdirectory(tests)

#######################################################################
install(FILES   "${CMAKE_CURRENT_SOURCE_DIR}/counters.c"
        DESTINATION "${KEDR_EXAMPLE_PATH}/counters"
)

install(FILES   "${CMAKE_CURRENT_SOURCE_DIR}/counters.h"
        DESTINATION "${KEDR_EXAMPLE_PATH}/counters"
)

install(FILES   "${CMAKE_CURRENT_SOURCE_DIR}/README"
        DESTINATION "${KEDR_EXAMPLE_PATH}/counters"
)

install(FILES   "${CMAKE_CURRENT_BINARY_DIR}/makefile_installed"
        DESTINATION "${KEDR_EXAMPLE_PATH}/counters"
        RENAME makefile
)

install(FILES   "${CMAKE_CURRENT_BINARY_DIR}/Kbuild_installed"
        DESTINATION "${KEDR_EXAMPLE_PATH}/counters"
        RENAME Kbuild
)
