set(module_name "kedr_counters")

kedr_load_install_prefixes()
set(KEDR_INCLUDE_DIR "${KEDR_INSTALL_INCLUDE_DIR}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Kbuild.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Kbuild_installed"
    @ONLY
)

set(kedr_symbols "${KEDR_INSTALL_PREFIX_KSYMVERS}/kedr.symvers")
set(kedr_gen_tool "${KEDR_INSTALL_PREFIX_EXEC_AUX}/kedr_gen")
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
            
            "${CMAKE_CURRENT_BINARY_DIR}/functions_common.data"
            "${CMAKE_CURRENT_BINARY_DIR}/configure_kernel_functions.sh"
            "${CMAKE_CURRENT_BINARY_DIR}/mutex_lock.data"
            "${CMAKE_CURRENT_BINARY_DIR}/mutex_lock_interruptible.data"
            "${CMAKE_CURRENT_BINARY_DIR}/mutex_lock_killable.data"
)

kedr_load_test_prefixes()
set(KEDR_INCLUDE_DIR "${KEDR_TEST_INCLUDE_DIR}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Kbuild.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
    @ONLY
)

set(kedr_symbols "${CMAKE_BINARY_DIR}/core/Module.symvers")
set(kedr_gen_tool "${KEDR_GEN_INSTALL_PREFIX}/kedr_gen")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/makefile.in"
    "${CMAKE_CURRENT_BINARY_DIR}/makefile_test"
    @ONLY
)

rule_copy_source("counters.c")
rule_copy_source("counters.h")

rule_copy_source("functions_common.data")
rule_copy_source("configure_kernel_functions.sh")

rule_copy_source("mutex_lock.data")
rule_copy_source("mutex_lock_interruptible.data")
rule_copy_source("mutex_lock_killable.data")

kedr_test_add_target("counters_module")

kedr_test_add_subdirectory(tests)

#######################################################################
install(FILES   "${CMAKE_CURRENT_SOURCE_DIR}/counters.c"
        DESTINATION "${KEDR_EXAMPLE_PATH}/counters"
)

install(FILES   "${CMAKE_CURRENT_SOURCE_DIR}/counters.h"
        DESTINATION "${KEDR_EXAMPLE_PATH}/counters"
)

install(FILES   "${CMAKE_CURRENT_SOURCE_DIR}/configure_kernel_functions.sh"
        DESTINATION "${KEDR_EXAMPLE_PATH}/counters"
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_WRITE GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

install(FILES   "${CMAKE_CURRENT_SOURCE_DIR}/functions_common.data"
                "${CMAKE_CURRENT_SOURCE_DIR}/mutex_lock.data"
                "${CMAKE_CURRENT_SOURCE_DIR}/mutex_lock_interruptible.data"
                "${CMAKE_CURRENT_SOURCE_DIR}/mutex_lock_killable.data"
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
