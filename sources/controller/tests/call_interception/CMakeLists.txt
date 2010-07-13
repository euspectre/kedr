set(kmodule_name target_call_all)
set(source_file "${CMAKE_CURRENT_BINARY_DIR}/${kmodule_name}.c")
set(template_dir "${CMAKE_CURRENT_SOURCE_DIR}/templates/target_call_all.c/") 

set(MIST_GEN_EXECUTABLE "${CMAKE_BINARY_DIR}/mist_gen/mist_gen")

file(WRITE ${function_triggers_file})

add_custom_command(OUTPUT ${source_file}
				COMMAND ${MIST_GEN_EXECUTABLE} ${template_dir} ${function_triggers_file} > ${source_file}
				DEPENDS ${function_triggers_file})

kbuild_add_module(${kmodule_name} ${source_file})
#add_custom_target(${kmodule_name} ALL
#				DEPENDS ${source_file})


#kedr_install_kmodule(${kmodule_name})
