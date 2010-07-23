set(mist_engine_build_dir "${CMAKE_CURRENT_BINARY_DIR}/mist_engine_build")
set(mist_engine_install_dir "${CMAKE_CURRENT_BINARY_DIR}/mist_engine")

file(MAKE_DIRECTORY ${mist_engine_build_dir} ${mist_engine_install_dir})

set(mist_inc_dir "${mist_engine_install_dir}/include")
set(mist_lib_dir "${mist_engine_install_dir}/lib")

execute_process(COMMAND cmake
							-DCMAKE_INSTALL_PREFIX=${mist_engine_install_dir}
							"${CMAKE_CURRENT_SOURCE_DIR}/src/mist_engine"
				WORKING_DIRECTORY ${mist_engine_build_dir}
				RESULT_VARIABLE mist_engine_result
				OUTPUT_VARIABLE mist_engine_output
				ERROR_VARIABLE mist_engine_output)

if(mist_engine_result EQUAL 0)
else(mist_engine_result EQUAL 0)
	message("cmake output:\n\n ${mist_engine_output}\n\n")
	message(FATAL_ERROR "cmake failed to configure mist engine")
endif(mist_engine_result EQUAL 0)


add_custom_target(mist_gen_executable ALL
				DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/mist_gen)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mist_gen
				COMMAND MIST_INC_DIR=${mist_inc_dir} MIST_LIB_DIR=${mist_lib_dir}
					make -C ${CMAKE_CURRENT_SOURCE_DIR}/src
				COMMAND cp -p ${CMAKE_CURRENT_SOURCE_DIR}/src/mist_gen
					${CMAKE_CURRENT_BINARY_DIR})
				
add_custom_target(mist_engine
				COMMAND make -C ${mist_engine_build_dir}
				COMMAND make -C ${mist_engine_build_dir} install)

add_dependencies(mist_gen_executable mist_engine)