# func_db_configure_functions([GENERATED] func1 func2 ...)
# Sets KEDR_DATA_FILE_* and KEDR_HEADER_DATA_FILE_* variables for 
# the listed functions, checks if the corresponding .data files exist.
# If "GENERATED" is not specified, the .data files for the functions 
# are looked for in the source directory, otherwise - in the binary 
# directory. 'header.data' files are currently always looked for in
# the source directory.
# 
# Use the form with "GENERATED" for the functions which .data files
# are generated (with configure_file() for example) and placed in the 
# build tree.
# 
# [NB] func_db_configure_functions() DOES NOT check if the functions are 
# actually present in the given system.
function(func_db_configure_functions)
	set(dir "${CMAKE_CURRENT_SOURCE_DIR}")
	set(to_check 1)
	foreach (func ${ARGN})
		if (func STREQUAL "GENERATED")
			set(to_check 0)
			set(dir "${CMAKE_CURRENT_BINARY_DIR}")
		else ()		
			set(KEDR_DATA_FILE_${func} "${dir}/${func}.data"
				CACHE INTERNAL
				"Path to the .data file for ${func}.")
			
			# Do not check the existence of the generated files as they
			# may be created a bit later...
			if (to_check)
				file(GLOB data_file_found "${KEDR_DATA_FILE_${func}}")
				if (NOT data_file_found)
					message(FATAL_ERROR 
					".data file is missing: ${KEDR_DATA_FILE_${func}}")
				endif (NOT data_file_found)
			endif (to_check)
			
			# Absolute path to 'header.data'. The file must always
			# be present.
			set(KEDR_HEADER_DATA_FILE_${func} 
				"${CMAKE_CURRENT_SOURCE_DIR}/header.data"
				CACHE INTERNAL
				"Path to the header.data file for ${func}.")
			file(GLOB hdata_file_found "${KEDR_HEADER_DATA_FILE_${func}}")
			if (NOT hdata_file_found)
				message(FATAL_ERROR 
				"The file is missing: ${KEDR_HEADER_DATA_FILE_${func}}}")
			endif (NOT hdata_file_found)
			file(APPEND "${KEDR_FUNC_DEF_FILE_IN}" 
				"#cmakedefine KEDR_FUNC_USED_${func}\n")
		endif (func STREQUAL "GENERATED")
	endforeach ()
endfunction(func_db_configure_functions)

add_subdirectory(common_mm)
add_subdirectory(virtual_mm)
add_subdirectory(mem_util)
add_subdirectory(uaccess)
add_subdirectory(capable)
add_subdirectory(mutexes)
add_subdirectory(schedule)
add_subdirectory(spinlocks)
add_subdirectory(waitqueue)
add_subdirectory(kasprintf)
