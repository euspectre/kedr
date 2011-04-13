add_custom_target("sample_target"
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${module_name}.ko"
                "${CMAKE_CURRENT_BINARY_DIR}/kedr_sample_target"
                )

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../makefile_installed.in"
	"${CMAKE_CURRENT_BINARY_DIR}/makefile_installed"
	@ONLY)

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${module_name}.ko"
        COMMAND $(MAKE) -f makefile_installed
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/cfake.c"
                "${CMAKE_CURRENT_BINARY_DIR}/cfake.h"
                "${CMAKE_CURRENT_BINARY_DIR}/makefile_installed"
                "${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
                )

rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/cfake.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/../cfake.c")
rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/cfake.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/../cfake.h")
rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
	"${CMAKE_CURRENT_SOURCE_DIR}/../Kbuild")
rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/kedr_sample_target"
	"${CMAKE_CURRENT_SOURCE_DIR}/../kedr_sample_target")

kedr_test_add_target("sample_target")


add_executable(test_sample_target EXCLUDE_FROM_ALL test_sample_target.c)

add_dependencies(build_tests test_sample_target)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/load_unload_simple.sh.in
	${CMAKE_CURRENT_BINARY_DIR}/load_unload_simple.sh
	@ONLY)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/load_unload_with_test.sh.in
	${CMAKE_CURRENT_BINARY_DIR}/load_unload_with_test.sh
	@ONLY)

kedr_test_add_script(examples.sample_target.01
	${CMAKE_CURRENT_BINARY_DIR}/load_unload_simple.sh)

kedr_test_add_script(examples.sample_target.02
	${CMAKE_CURRENT_BINARY_DIR}/load_unload_with_test.sh)
