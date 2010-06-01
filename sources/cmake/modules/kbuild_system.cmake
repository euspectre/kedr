# Where this CMake module is located
set(kbuild_this_module_dir "${CMAKE_SOURCE_DIR}/cmake/modules")
# Symvers files, which should be processed for build kernel module
set(kbuild_symbol_files)
#include directories for build kernel modules
set(kbuild_include_dirs)
# kbuild_add_module(name [sources ..])
#
# Build kernel module from sources_files, analogue of add_executable.
#
# Sources files are divided into two categories:
# -Object sources
# -Other sourses
#
# Object sources are thouse sources,
# which may be used in building kernel module externally.
# Follow types of object sources are supported now:
# .o: object file, do not require additional preprocessing.
# .c: c-file.
# (.S files will be added if required)
# 
# Other sources is treated as only prerequisite of building process.
#
# (!)All object sources are treated relative to the current dir,
# not to the current source dir.
#
# Only one call of kbuild_add_module or kbuild_add_objects
# is allowed in the CMakeLists.txt.
#
# In case when 'sources' omitted, module will be built 
# from "${name}.c" source.

function(kbuild_add_module name)
	#Global target
	add_custom_target(${name} ALL
			DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${name}.ko)
	#Sources
	if(ARGN)
		set(sources ${ARGN})
	else(ARGN)
		set(sources "${CMAKE_CURRENT_BINARY_DIR}/${name}.c")
	endif(ARGN)
	#Object sources with absolute path
	set(obj_sources_abs)
	#Other sources
	set(other_sources)
	foreach(source ${sources})
		string(REGEX MATCH ".+(\\.c)|(\\.o)$" is_obj_source ${source})
		if(is_obj_source)
			string(REGEX MATCH "^/" is_abs_path ${source})
			if(NOT is_abs_path)
				set(source "${CMAKE_CURRENT_BINARY_DIR}/${source}")
			endif(NOT is_abs_path)
			list(APPEND obj_sources_abs ${source})
		else(is_obj_source)
			list(APPEND other_sources ${source})
		endif(is_obj_source)
	endforeach(source ${sources})
	#list of files from which module building is depend
	set(depend_files ${obj_sources_abs} ${other_sources})
	#Sources of "c" type, but without extension
	#(for clean files)
	set(c_sources_noext_abs)
	#Sources of "o" type, but without extension
	set(o_sources_noext_abs)
	foreach(obj_source_abs ${obj_sources_abs})
		string(REGEX REPLACE "(.+)((\\.c)|(\\.o))$" "\\1"
			obj_source_noext_abs
			${obj_source_abs})
		if(CMAKE_MATCH_2 STREQUAL ".c")
			list(APPEND c_sources_noext_abs ${obj_source_noext_abs})
		elseif(CMAKE_MATCH_2 STREQUAL ".o")
			list(APPEND o_sources_noext_abs ${obj_source_noext_abs})
		else(CMAKE_MATCH_2 STREQUAL ".c")
			message(FATAL_ERROR "BUG in kbuild_add_module")
		endif(CMAKE_MATCH_2 STREQUAL ".c")
	endforeach(obj_source_abs ${obj_sources_abs})
	#Object sources relative to current dir
	#(for $(module)-y :=)
	set(obj_sources_noext_rel)
	foreach(source_noext_abs ${c_sources_noext_abs} ${o_sources_noext_abs})
		file(RELATIVE_PATH source_noext_rel ${CMAKE_CURRENT_BINARY_DIR} ${source_noext_abs})			
		list(APPEND obj_sources_noext_rel ${source_noext_rel})
	endforeach(source_noext_abs ${c_sources_noext_abs} ${o_sources_noext_abs})
	if(NOT obj_sources_noext_rel)
		message(FATAL_ERROR "List of object files for module ${name} is empty.")
	endif(NOT obj_sources_noext_rel)
	#Detect, if build simple - source object name coincide with module name
	if(obj_sources_noext_rel STREQUAL ${name})
		set(is_build_simple "TRUE")
	else(obj_sources_noext_rel STREQUAL ${name})
		#Detect, if only one of source object names coincide with module name.
		#This situation is incorrect for kbuild system.
		list(FIND obj_sources_noext_rel ${name} is_objects_contain_name)
		if(is_objects_contain_name GREATER -1)
			message(FATAL_ERROR "Module should be built"
			"either from only one object with same name,"
			"or from objects with names, different from module name")
		endif(is_objects_contain_name GREATER -1)
		set(is_build_simple "FALSE")
	endif(obj_sources_noext_rel STREQUAL ${name})
	#List of files for deleting in 'make clean'
	set(clean_files_list)
	_kbuild_add_clean_files_common(clean_files_list)
	_kbuild_add_clean_files_module(${name} clean_files_list)
	foreach(c_source_noext_abs ${c_sources_noext_abs})
		_kbuild_add_clean_files_c(${c_source_noext_abs} clean_files_list)
		list(APPEND clean_files_list "${c_source_noext_abs}.o")
	endforeach(c_source_noext_abs ${c_sources_noext_abs})
	#Build kbuild file - module string
	set(obj_string "obj-m := ${name}.o")
	#Build kbuild file - object sources string
	if(is_build_simple)
		set(obj_src_string "")
	else(is_build_simple)
		set(obj_src_string "${name}-y := ")
		foreach(obj ${obj_sources_noext_rel})
			set(obj_src_string "${obj_src_string} ${obj}.o")
		endforeach(obj ${obj_sources_noext_rel})
	endif(is_build_simple)
	#Build kbuild file - compiler flags
	set(cflags_string "ccflags-y := ")
	#compiler flags - directories
	if(kbuild_include_dirs)
		foreach(dir ${kbuild_include_dirs})
			set(cflags_string "${cflags_string} -I${dir}")
		endforeach(dir ${kmodule_include_dirs})
	endif(kbuild_include_dirs)
	#Configure kbuild file
	configure_file(${kbuild_this_module_dir}/kbuild_system_files/Kbuild.in
					${CMAKE_CURRENT_BINARY_DIR}/Kbuild
					)
	#Create rules, depending on kbuild_use_symbols call
	set(symvers_file ${CMAKE_CURRENT_BINARY_DIR}/Module.symvers)
	if(kbuild_symbol_files)
		list(APPEND depend_files ${symvers_file})

		add_custom_command(OUTPUT ${symvers_file}
				COMMAND cat ${kbuild_symbol_files} >> ${symvers_file}
				DEPENDS ${kbuild_symbol_files}
				)
	else(kbuild_symbol_files)
		list(APPEND clean_files_list ${symvers_file})
	endif(kbuild_symbol_files)
	add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${name}.ko
			COMMAND make -C ${KBUILD_BUILD_DIR} M=${CMAKE_CURRENT_BINARY_DIR} modules
			DEPENDS ${depend_files}
			)

	set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${clean_files_list}")
endfunction(kbuild_add_module name)

# kbuild_add_object(source [dependences..])
#
# Build kernel object file from source.
# These objects may be used for build kernel module.

# Source file- c-file(filename.c), from which object file should be built.
# This file is treated as file in ${CMAKE_CURRENT_BINARY_DIR} directory.
# Also, target with name of this file(without extension) is created.

# Only one call of kbuild_add_module or kbuild_add_object
# is allowed in the CMakeLists.txt.
function(kbuild_add_object source)
	# Extract name of file
	string(REGEX MATCH "^([^/]+)\\.c$" is_correct_filename ${source})
	if(NOT is_correct_filename)
		message(FATAL_ERROR "kbuild_add_object: 'source' should be c-file without directory part.")
	endif(NOT is_correct_filename)
	set(name ${CMAKE_MATCH_1})
	set(depend_files "${CMAKE_CURRENT_BINARY_DIR}/${source}" ${ARGN})
	# Create global rule
	add_custom_target(${name} ALL
				DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${name}.o")
	#Files for clean
	set(clean_files_list)
	_kbuild_add_clean_files_common(clean_files_list)
	_kbuild_add_clean_files_object(clean_files_list)
	_kbuild_add_clean_files_c(${CMAKE_CURRENT_BINARY_DIR}/${name} clean_files_list)
	#Build object file - objects str
	set(objects_string "obj-y := ${name}.o")
	#Build kbuild file - compiler flags
	set(cflags_string "ccflags-y := ")
	#compiler flags - directories
	if(kbuild_include_dirs)
		foreach(dir ${kbuild_include_dirs})
			set(cflags_string "${cflags_string} -I${dir}")
		endforeach(dir ${kmodule_include_dirs})
	endif(kbuild_include_dirs)
	#Configure kbuild file
	configure_file(${kbuild_this_module_dir}/kbuild_system_files/Kbuild_object.in
					${CMAKE_CURRENT_BINARY_DIR}/Kbuild
					)
	#create rules
	list(APPEND clean_files_list "${CMAKE_CURRENT_BINARY_DIR}/Module.symvers")
	add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${name}.o"
			COMMAND make -C ${KBUILD_BUILD_DIR} M=${CMAKE_CURRENT_BINARY_DIR}
			DEPENDS ${depend_files}
			)

	set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${clean_files_list}")
endfunction(kbuild_add_object source)

#kbuild_include_directories(dir1 .. dirn)
macro(kbuild_include_directories)
	list(APPEND kbuild_include_dirs ${ARGN})
endmacro(kbuild_include_directories)
#kbuild_use_symbols(symvers_file1.. symvers_filen)
macro(kbuild_use_symbols)
#	set(kbuild_symbol_files "${kbuild_symbol_files} ${ARGN}")
	list(APPEND kbuild_symbol_files ${ARGN})
endmacro(kbuild_use_symbols)

# Internal functions
# List common files, created by kbuild
macro(_kbuild_add_clean_files_common clean_files_list)
	list(APPEND ${clean_files_list}
		"${CMAKE_CURRENT_BINARY_DIR}/.tmp_versions"
		"${CMAKE_CURRENT_BINARY_DIR}/modules.order" 
		"${CMAKE_CURRENT_BINARY_DIR}/Module.markers")
endmacro(_kbuild_add_clean_files_common clean_files_list)
# List common files, created by kbuild when built only object
macro(_kbuild_add_clean_files_object clean_files_list)
	list(APPEND ${clean_files_list}
		"${CMAKE_CURRENT_BINARY_DIR}/built-in.o"
		"${CMAKE_CURRENT_BINARY_DIR}/.built-in.o.cmd" 
		"${CMAKE_CURRENT_BINARY_DIR}/Module.markers")

endmacro(_kbuild_add_clean_files_object clean_files_list)
# List module name-depended files, created by kbuild
macro(_kbuild_add_clean_files_module name clean_files_list)
	foreach(created_file
		"${name}.o" "${name}.mod.c" "${name}.mod.o"
		".${name}.ko.cmd" ".${name}.mod.o.cmd" ".${name}.o.cmd")
		list(APPEND ${clean_files_list}
			"${CMAKE_CURRENT_BINARY_DIR}/${created_file}")
	endforeach(created_file
			"${name}.o" "${name}.mod.c" "${name}.mod.o"
		".${name}.ko.cmd" ".${name}.mod.o.cmd" ".${name}.o.cmd")
	string(REPLACE ";" "      " clean_files_list_str ${clean_files_list})
endmacro(_kbuild_add_clean_files_module module_name clean_files_list)
# List files, created when kbuild compile c-files into o-files
macro(_kbuild_add_clean_files_c c_source_noext_abs clean_files_list)
	string(REGEX MATCH "^(.+/)([^/]+)" _kbuild_correct_filename ${c_source_noext_abs})
	if(NOT _kbuild_correct_filename)
		message(FATAL_ERROR "Incorrect format of filename: '${c_source_noext}'")
	endif(NOT _kbuild_correct_filename)
	set(_kbuild_dir_part ${CMAKE_MATCH_1})
	set(_kbuild_name ${CMAKE_MATCH_2})
	foreach(created_file "${_kbuild_name}.o" ".${_kbuild_name}.o.cmd"
		".${_kbuild_name}.o.d" #this file is exist in case of unsuccessfull build
		)
		list(APPEND ${clean_files_list} "${_kbuild_dir_part}${created_file}")
	endforeach(created_file "${_kbuild_name}.o" ".${_kbuild_name}.o.cmd")
endmacro(_kbuild_add_clean_files_c c_source_noext_abs clean_files_list)

#auxiliary
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