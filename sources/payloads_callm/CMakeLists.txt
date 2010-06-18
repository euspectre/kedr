include(cmake_useful)

set(CONTROLLER_SYMVERS_FILE "${CMAKE_BINARY_DIR}/controller/Module.symvers")
kbuild_use_symbols(${CONTROLLER_SYMVERS_FILE})

set(MIST_GEN_EXECUTABLE "${CMAKE_BINARY_DIR}/mist_gen/mist_gen")
set(MIST_GEN_TEMPLATES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/templates")
# Common commands for generate payload.h and trace_payload.h
function(rules_generate_payload data_file dest_dir)
	add_custom_command(OUTPUT ${dest_dir}/payload.c
				COMMAND ${MIST_GEN_EXECUTABLE} ${MIST_GEN_TEMPLATES_DIR}/payload.c/ ${data_file} > ${dest_dir}/payload.c
				DEPENDS ${data_file})
	add_custom_command(OUTPUT ${dest_dir}/trace_payload.h
				COMMAND ${MIST_GEN_EXECUTABLE} ${MIST_GEN_TEMPLATES_DIR}/trace_payload.h/ ${data_file} > ${dest_dir}/trace_payload.h
				DEPENDS ${data_file})
endfunction(rules_generate_payload data_file dest_dir)

#rules_build_datafile(data_file data_begin [data_func1...])
function(rules_build_datafile data_file)
	to_abs_path(data_funcs_abs ${ARGN})
	add_custom_command(OUTPUT ${data_file}
						COMMAND cat ${data_funcs_abs} > ${data_file}
						DEPENDS ${data_funcs_abs})
endfunction(rules_build_datafile data_file)

# Helper for configure list of pieces of data-file.
# Similar to kmodule_configure_kernel_functions, but special for produce list of the pieces of the data-file.
#
# kedr_configure_data_pieces(output_filelist REQUIRED | OPTIONAL | func ...)
#
# If ${func} function exist in the kernel, then "${func}.data" is added to the output_filelist.
#
# There are 2 modes for lookup function: 
# OPTIONAL - if functions doesn't exists, it silently ignored.
# REQUIRED - if functions doesn't exists, FATAL_ERROR message is printed.
#
# Initial mode is REQUIRED, and it can be changed at any time by REQUIRED and OPTIONAL.
#
# ONE_OF_BEGIN/ONE_OF_END determine section, in which no more than one function should exist,
# otherwise FATAL_ERROR message is printed. When mode is REQUIRED, precisely one function should exist.
# Inside this section other keywords shouldn't be used (even another ONE_OF_BEGIN).

macro(kedr_configure_data_pieces output_filelist)
	set(kedr_configure_data_pieces_mode "REQUIRED")
	set(kedr_configure_data_pieces_one_of_section)
	set(${output_filelist})
	foreach(arg ${ARGN})
		if(arg STREQUAL "REQUIRED")
			if(kedr_configure_data_pieces_one_of_section)
				message(FATAL_ERROR "Inside ONE_OF_BEGIN/ONE_OF_END section other keywords are not allowed.")
			endif(kedr_configure_data_pieces_one_of_section)
			set(kedr_configure_data_pieces_mode "REQUIRED")
		elseif(arg STREQUAL "OPTIONAL")
			if(kedr_configure_data_pieces_one_of_section)
				message(FATAL_ERROR "Inside ONE_OF_BEGIN/ONE_OF_END section other keywords are not allowed.")
			endif(kedr_configure_data_pieces_one_of_section)
			set(kedr_configure_data_pieces_mode "OPTIONAL")
		elseif(arg STREQUAL "ONE_OF_BEGIN")
			if(kedr_configure_data_pieces_one_of_section)
				message(FATAL_ERROR "Nested ONE_OF_BEGIN/ONE_OF_END sections are not allowed.")
			endif(kedr_configure_data_pieces_one_of_section)
			set(kedr_configure_data_pieces_one_of_section "TRUE")
			set(kedr_configure_data_pieces_one_of_section_file)
		elseif(arg STREQUAL "ONE_OF_END")
			if(NOT kedr_configure_data_pieces_one_of_section)
				message(FATAL_ERROR "ONE_OF_END without ONE_OF_BEGIN is not allowed.")
			endif(NOT kedr_configure_data_pieces_one_of_section)
			if(NOT kedr_configure_data_pieces_one_of_section_file AND kedr_configure_data_pieces_mode STREQUAL "REQUIRED")
				message(FATAL_ERROR "No any function in ONE_OF section exists in the kernel, but it is required.")
			endif(NOT kedr_configure_data_pieces_one_of_section_file AND kedr_configure_data_pieces_mode STREQUAL "REQUIRED")
			list(APPEND ${output_filelist} ${kedr_configure_data_pieces_one_of_section_file})
			set(kedr_configure_data_pieces_one_of_section)
		else(arg STREQUAL "REQUIRED")
			kmodule_configure_kernel_functions(kedr_configure_data_pieces_file
				${arg}	"${arg}.data")
			if(NOT kedr_configure_data_pieces_one_of_section)
				if(NOT kedr_configure_data_pieces_file AND kedr_configure_data_pieces_mode STREQUAL "REQUIRED")
					message(FATAL_ERROR "Function ${arg} is absent in the kernel, but it is required.")
				endif(NOT kedr_configure_data_pieces_file AND kedr_configure_data_pieces_mode STREQUAL "REQUIRED")
				list(APPEND ${output_filelist} ${kedr_configure_data_pieces_file})
			else(NOT kedr_configure_data_pieces_one_of_section)
				if(kedr_configure_data_pieces_file AND kedr_configure_data_pieces_one_of_section_file)
					message(FATAL_ERROR "Two functions from ONE_OF sections exist in the kernel.")
				endif(kedr_configure_data_pieces_file AND kedr_configure_data_pieces_one_of_section_file)
				list(APPEND kedr_configure_data_pieces_one_of_section_file ${kedr_configure_data_pieces_file})
			endif(NOT kedr_configure_data_pieces_one_of_section)
		endif(arg STREQUAL "REQUIRED")
	endforeach(arg ${ARGN})
endmacro(kedr_configure_data_pieces output_filelist)

#add_subdirectory(my_payload1)
add_subdirectory(common_memory_management)
add_subdirectory(mutexes)
add_subdirectory(spinlocks)
add_subdirectory(user_space_access)
add_subdirectory(shedule)
add_subdirectory(waitqueue)