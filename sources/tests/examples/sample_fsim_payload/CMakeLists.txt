add_custom_target("sample_fsim_payload"
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${module_name}.ko"
                )

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${module_name}.ko"
        COMMAND $(MAKE) -f makefile_test
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/payload.c"
                "${CMAKE_CURRENT_BINARY_DIR}/functions.data"
                "${CMAKE_CURRENT_BINARY_DIR}/makefile_test"
                "${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
                )

set(KEDR_INCLUDE_DIR "${KEDR_TEST_INCLUDE_DIR}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../Kbuild.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
    @ONLY
)

set(kedr_symbols "${CMAKE_BINARY_DIR}/core/Module.symvers")
set(fault_simulation_symbols "${CMAKE_BINARY_DIR}/fault_simulation/Module.symvers")
set(kedr_gen_tool "${KEDR_GEN_INSTALL_PREFIX}/kedr_gen")
set(kedr_gen_templates_dir "${CMAKE_SOURCE_DIR}/templates")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../makefile.in"
    "${CMAKE_CURRENT_BINARY_DIR}/makefile_test"
    @ONLY
)

rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/payload.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../payload.c")
rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/functions.data"
    "${CMAKE_CURRENT_SOURCE_DIR}/../functions.data")

kedr_test_add_target("sample_fsim_payload")
