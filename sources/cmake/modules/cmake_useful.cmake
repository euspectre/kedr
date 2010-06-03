#Create rule for obtain one file by copiing another
function(rule_copy_file target_file source_file)
	add_custom_command(OUTPUT ${target_file}
					COMMAND cp -p ${source_file} ${target_file}
					DEPENDS ${source_file}
					)
endfunction(rule_copy_file target_file source_file)
#Create rule for obtain file in binary tree by copiing it from source tree
function(rule_copy_source rel_source_file)
	rule_copy_file(${CMAKE_CURRENT_BINARY_DIR}/${rel_source_file} ${CMAKE_CURRENT_SOURCE_DIR}/${rel_source_file})
endfunction(rule_copy_source rel_source_file)
# to_abs_path(output_var path [...])
#
# Convert relative path of file to absolute path:
# use path in source tree, if file already exist there.
# otherwise use path in binary tree.
# If initial path already absolute, return it.
macro(to_abs_path output_var)
	set(${output_var})
	foreach(path ${ARGN})
		string(REGEX MATCH "^/" _is_abs_path ${path})
		if(_is_abs_path)
			list(APPEND ${output_var} ${path})
		else(_is_abs_path)
			file(GLOB _is_file_exist_in_source
				${CMAKE_CURRENT_SOURCE_DIR}/${path})
			if(_is_file_exist_in_source)
				list(APPEND ${output_var} ${CMAKE_CURRENT_SOURCE_DIR}/${path})
			else(_is_file_exist_in_source)
				list(APPEND ${output_var} ${CMAKE_CURRENT_BINARY_DIR}/${path})
			endif(_is_file_exist_in_source)
		endif(_is_abs_path)
	endforeach(path ${ARGN})
endmacro(to_abs_path output_var path)
#is_path_inside_dir(output_var dir path)
#
# Set output_var to true if path is absolute path inside given directory.
# (!) path should be absolute.
macro(is_path_inside_dir output_var dir path)
	file(RELATIVE_PATH _rel_path ${dir} ${path})
	string(REGEX MATCH "^\\.\\." _is_not_inside_dir ${_rel_path})
	if(_is_not_inside_dir)
		set(${output_var} "FALSE")
	else(_is_not_inside_dir)
		set(${output_var} "TRUE")
	endif(_is_not_inside_dir)
endmacro(is_path_inside_dir output_var dir path)
