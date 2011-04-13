add_custom_target("counters_module"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${module_name}.ko"
)

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${module_name}.ko"
    COMMAND $(MAKE) -f makefile_test
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/counters.c"
            "${CMAKE_CURRENT_BINARY_DIR}/counters.h"
            "${CMAKE_CURRENT_BINARY_DIR}/makefile_test"
            "${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
            
            "${CMAKE_CURRENT_BINARY_DIR}/functions_common.data"
            "${CMAKE_CURRENT_BINARY_DIR}/configure_kernel_functions.sh"
            "${CMAKE_CURRENT_BINARY_DIR}/mutex_lock.data"
            "${CMAKE_CURRENT_BINARY_DIR}/mutex_lock_interruptible.data"
            "${CMAKE_CURRENT_BINARY_DIR}/mutex_lock_killable.data"
)

kedr_load_test_prefixes()
set(KEDR_INCLUDE_DIR "${KEDR_TEST_INCLUDE_DIR}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../Kbuild.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
    @ONLY
)

set(kedr_symbols "${CMAKE_BINARY_DIR}/core/Module.symvers")
set(kedr_gen_tool "${KEDR_GEN_INSTALL_PREFIX}/kedr_gen")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../makefile.in"
    "${CMAKE_CURRENT_BINARY_DIR}/makefile_test"
    @ONLY
)

rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/counters.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../counters.c")
rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/counters.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../counters.h")

rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/functions_common.data"
    "${CMAKE_CURRENT_SOURCE_DIR}/../functions_common.data")

rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/configure_kernel_functions.sh"
    "${CMAKE_CURRENT_SOURCE_DIR}/../configure_kernel_functions.sh")


rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/mutex_lock.data"
    "${CMAKE_CURRENT_SOURCE_DIR}/../mutex_lock.data")

rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/mutex_lock_interruptible.data"
    "${CMAKE_CURRENT_SOURCE_DIR}/../mutex_lock_interruptible.data")

rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/mutex_lock_killable.data"
    "${CMAKE_CURRENT_SOURCE_DIR}/../mutex_lock_killable.data")


kedr_test_add_target("counters_module")

#######################################################################

set (KEDR_TOOLS_DIR "${CMAKE_BINARY_DIR}/tools")

# '@ONLY' is essential when doing substitutions in the shell scripts. 
# Without it, CMake would replace "${...}" too, which is usually not what 
# you want.
configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/test_counters.sh.in"
  "${CMAKE_CURRENT_BINARY_DIR}/test_counters.sh"
  @ONLY
)

configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/counters.conf.in"
  "${CMAKE_CURRENT_BINARY_DIR}/counters.conf"
  @ONLY
)

kedr_test_add_script (examples.counters.01 
    test_counters.sh
)

