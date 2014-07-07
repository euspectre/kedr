add_custom_target("example_lc_annotations"
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${module_name}.ko"
)

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${module_name}.ko"
        COMMAND $(MAKE) -f makefile_test
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/makefile_test"
                "${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
                "${CMAKE_CURRENT_BINARY_DIR}/module.c"
                "${CMAKE_CURRENT_BINARY_DIR}/cfake.h"
)

rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/cfake.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/../cfake.h"
)

rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/module.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/../module.c"
)

kedr_load_test_prefixes()
set(KEDR_INCLUDE_DIR "${KEDR_TEST_INCLUDE_DIR}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../Kbuild.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
    @ONLY
)

set(kedr_leak_check_symbols "${CMAKE_BINARY_DIR}/leak_check/core/Module.symvers")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../makefile.in"
    "${CMAKE_CURRENT_BINARY_DIR}/makefile_test"
    @ONLY
)

kedr_test_add_target("example_lc_annotations")
