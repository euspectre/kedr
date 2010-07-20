set(module_name "kedr_sample_target")

#For testing
add_custom_target("sample_target"
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${module_name}.ko"
                "${CMAKE_CURRENT_BINARY_DIR}/kedr_sample_target"
                )

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${module_name}.ko"
        COMMAND make -f makefile_installed
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/cfake.c"
                "${CMAKE_CURRENT_BINARY_DIR}/cfake.h"
                "${CMAKE_CURRENT_BINARY_DIR}/makefile_installed"
                "${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
                )

rule_copy_source("cfake.c")
rule_copy_source("cfake.h")
rule_copy_source("Kbuild")
rule_copy_source("makefile_installed")
rule_copy_source("kedr_sample_target")

add_dependencies("build_tests" "sample_target")



install(FILES   "${CMAKE_CURRENT_SOURCE_DIR}/cfake.h"
                "${CMAKE_CURRENT_SOURCE_DIR}/cfake.c"
                "${CMAKE_CURRENT_SOURCE_DIR}/Kbuild"
        DESTINATION "${KEDR_EXAMPLE_PATH}/sample_target"
)
install(FILES   "${CMAKE_CURRENT_SOURCE_DIR}/kedr_sample_target"
        DESTINATION "${KEDR_EXAMPLE_PATH}/sample_target"
        PERMISSIONS  OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ OWNER_EXECUTE GROUP_EXECUTE WORLD_EXECUTE
)
install(FILES   "${CMAKE_CURRENT_SOURCE_DIR}/makefile_installed"
        DESTINATION "${KEDR_EXAMPLE_PATH}/sample_target"
        RENAME makefile
)

add_subdirectory(tests)