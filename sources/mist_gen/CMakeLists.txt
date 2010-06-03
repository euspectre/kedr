add_custom_target(mist_gen_executable ALL
				DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/mist_gen)
add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/mist_gen
				COMMAND make -C ${CMAKE_CURRENT_SOURCE_DIR} -f makefile.linux)