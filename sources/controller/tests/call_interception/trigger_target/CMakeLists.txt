set(kmodule_name trigger_target)
set(source_file "${CMAKE_CURRENT_BINARY_DIR}/${kmodule_name}.c")

add_custom_command(OUTPUT ${source_file}
				COMMAND ${KEDR_GEN_TOOL} "${templates_dir}/trigger_target.c" ${function_triggers_file} > ${source_file}
				DEPENDS ${function_triggers_file}
)

kbuild_add_module(${kmodule_name} ${source_file})

kedr_test_add_target(${kmodule_name})
