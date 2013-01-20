include(cmake_useful)

# This environment variable can be used in Makefiles
set (ENV{KERNELDIR} "${KBUILD_BUILD_DIR}")

# Location of this CMake module
set(kbuild_this_module_dir "${CMAKE_SOURCE_DIR}/cmake/modules")

# Symvers files to be processed for building the kernel module
set(kbuild_symbol_files)

# Names of the targets the built kernel module should depend on.
# Usually, these are the targets used for building some other kernel 
# modules which symvers files are used via kbuild_use_symbols().
set(kbuild_dependencies_modules)

# #include directories for building kernel modules
set(kbuild_include_dirs)

# Additional compiler flags for the module
set(kbuild_cflags)

# Helper for the building kernel module.
# 
# Test whether given source file is inside source tree. If it is so,
# create rule for copy file to same relative location in binary tree.
# Output variable will contain file's absolute path in binary tree.

function(copy_source_to_binary_tree source new_source_var)
	is_path_inside_dir(is_in_source ${CMAKE_SOURCE_DIR} "${source}")
	is_path_inside_dir(is_in_binary ${CMAKE_BINARY_DIR} "${source}")
	if(is_in_source AND NOT is_in_binary)
		#special process c-sources in source tree
		file(RELATIVE_PATH source_rel "${CMAKE_SOURCE_DIR}" "${source}")
		set(new_source "${CMAKE_BINARY_DIR}/${source_rel}")
		#add rule for create duplicate..
		rule_copy_file("${new_source}" "${source}")
	else(is_in_source AND NOT is_in_binary)
		set(new_source "${source}")
	endif(is_in_source AND NOT is_in_binary)
	set(${new_source_var} "${new_source}" PARENT_SCOPE)
endfunction(copy_source_to_binary_tree source new_source_var)

# Extract components of the file.
#
# file_components(filepath [<component> RESULT_VAR])
# 
# For each <component>, set RESULT_VAR to corresponded component of filepath.
#
# <component> may be:
#	- DIR: everything until last '/' (including it) if it exists.
#   - NOTDIR: everything except directory(without '/').
#	- SUFFIX: last suffix(including '.'), if it exists.
#   - BASENAME: everything without suffix (without '.').


function(file_components filepath)
	set(state "None")
	foreach(arg ${ARGN})
		if(arg STREQUAL "DIR")
			set(state "DIR")
		elseif(arg STREQUAL "NOTDIR")
			set(state "NOTDIR")
		elseif(arg STREQUAL "SUFFIX")
			set(state "SUFFIX")
		elseif(arg STREQUAL "BASENAME")
			set(state "BASENAME")
		elseif(state STREQUAL "DIR")
			STRING(REGEX REPLACE "[^/]+$" "" dir ${filepath})
			set(${arg} "${dir}" PARENT_SCOPE)
		elseif(state STREQUAL "NOTDIR")
			STRING(REGEX REPLACE ".*/" "" notdir ${filepath})
			set(${arg} "${notdir}" PARENT_SCOPE)
		elseif(state STREQUAL "SUFFIX")
			STRING(REGEX MATCH "\\.[^/\\.]+$" suffix ${filepath})
			set(${arg} "${suffix}" PARENT_SCOPE)
		elseif(state STREQUAL "BASENAME")
			STRING(REGEX REPLACE "\\.[^/\\.]+$" "" basename ${filepath})
			set(${arg} "${basename}" PARENT_SCOPE)
		else(arg STREQUAL "DIR")
			message(FATAL "Unexpected argument: ${arg}")
		endif(arg STREQUAL "DIR")
	endforeach(arg ${ARGN})
endfunction(file_components filepath)

# kbuild_add_module(name [sources ...])
#
# Build a kernel module from sources_files, similar to add_executable().
#
# The source files are divided into two categories:
# - object sources;
# - other sources.
#
# Object sources are the sources that can be used when building the kernel 
# module externally.
# The following types of object sources are currently supported:
# .c: a file with the code in C language;
# .S: a file with the code in assembly language;
# .o_shipped: shipped file in binary format,
#             does not require additional preprocessing.
# 
# Other sources are treated only as the prerequisites in the build process.
#
# Only one call to kbuild_add_module() or kbuild_add_objects()
# is allowed in the CMakeLists.txt.
#
# In case when 'sources' are omitted, the module will be built from 
# "${name}.c" source file.

function(kbuild_add_module name)
    # Enable Sparse if requested
    if (KEDR_USE_SPARSE)
    	set(kedr_sparse_check "C=1")
    else ()
    	set(kedr_sparse_check "")
    endif (KEDR_USE_SPARSE)
    
    set(symvers_file ${CMAKE_CURRENT_BINARY_DIR}/Module.symvers)
	# Global target
	add_custom_target(${name} ALL
			DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${name}.ko ${symvers_file})
	if(kbuild_dependencies_modules)
		add_dependencies(${name} ${kbuild_dependencies_modules})
	endif(kbuild_dependencies_modules)
	# Sources
	if(ARGN)
		set(sources ${ARGN})
	else(ARGN)
		set(sources "${CMAKE_CURRENT_BINARY_DIR}/${name}.c")
	endif(ARGN)
	# Sources with absolute paths
	to_abs_path(sources_abs ${sources})
	# The list of files the building of the module depends on
	set(depend_files)
	# Sources of "c" type, but without extension
	# (for clean files, 
	# for out-of-source builds do not create files in source tree)
	set(c_sources_noext_abs)
	# The sources with the code in assembly
	set(asm_sources_noext_abs)
	# Sources of "o_shipped" type, but without extension
	set(shipped_sources_noext_abs)
	# Sort the sources and move them into binary tree if needed
	foreach(source_abs ${sources_abs})
		file_components(${source_abs} SUFFIX ext)
		if(ext STREQUAL ".c" OR ext STREQUAL ".S" OR ext STREQUAL ".o_shipped")
			# Real sources
			# Move source into binary tree, if needed
			copy_source_to_binary_tree("${source_abs}" source_abs)
			if(ext STREQUAL ".c")
				# c-source
				file_components("${source_abs}" BASENAME c_source_noext_abs)
				list(APPEND c_sources_noext_abs ${c_source_noext_abs})
			elseif(ext STREQUAL ".S")
				# asm source
				file_components("${source_abs}" BASENAME asm_source_noext_abs)
				list(APPEND asm_sources_noext_abs ${asm_source_noext_abs})
			elseif(ext STREQUAL ".o_shipped")
				# shipped-source
				file_components("${source_abs}" BASENAME shipped_source_noext_abs)
				list(APPEND shipped_sources_noext_abs ${shipped_source_noext_abs})
			endif(ext STREQUAL ".c")
		endif(ext STREQUAL ".c" OR ext STREQUAL ".S" OR ext STREQUAL ".o_shipped")
		# In any case, add file to depend list
		list(APPEND depend_files ${source_abs})
	endforeach(source_abs ${sources_abs})
	# Form the relative paths to the sources
	#(for $(module)-y :=)
	set(c_sources_noext_rel)
	foreach(c_source_noext_abs ${c_sources_noext_abs})
		file(RELATIVE_PATH c_source_noext_rel
			${CMAKE_CURRENT_BINARY_DIR} ${c_source_noext_abs})
		list(APPEND c_sources_noext_rel ${c_source_noext_rel})
	endforeach(c_source_noext_abs ${c_sources_noext_abs})
	
	set(asm_sources_noext_rel)
	foreach(asm_source_noext_abs ${asm_sources_noext_abs})
		file(RELATIVE_PATH asm_source_noext_rel
			${CMAKE_CURRENT_BINARY_DIR} ${asm_source_noext_abs})
		list(APPEND asm_sources_noext_rel ${asm_source_noext_rel})
	endforeach(asm_source_noext_abs ${asm_sources_noext_abs})
	
	set(shipped_sources_noext_rel)
	foreach(shipped_source_noext_abs ${shipped_sources_noext_abs})
		file(RELATIVE_PATH shipped_source_noext_rel
			${CMAKE_CURRENT_BINARY_DIR} ${shipped_source_noext_abs})
		list(APPEND shipped_sources_noext_rel ${shipped_source_noext_rel})
	endforeach(shipped_source_noext_abs ${shipped_sources_noext_abs})
	
	# Join all sources to determine type of the build (simple or not)
	set(obj_sources_noext_rel 
		${c_sources_noext_rel} 
		${asm_sources_noext_rel} 
		${shipped_sources_noext_rel})

	if(NOT obj_sources_noext_rel)
		message(FATAL_ERROR "The list of object files for module \"${name}\" is empty.")
	endif(NOT obj_sources_noext_rel)
	# Detect if the build is simple, i.e the source object name is the same as 
	# the module name
	if(obj_sources_noext_rel STREQUAL ${name})
		set(is_build_simple "TRUE")
	else(obj_sources_noext_rel STREQUAL ${name})
		# Detect if some of source object names are the same as the module name 
		# and there are the source object files except these.
		# This situation is incorrect for kbuild system.
		list(FIND obj_sources_noext_rel ${name} is_objects_contain_name)
		if(is_objects_contain_name GREATER -1)
			message(FATAL_ERROR "Module should be built "
			"either from a single object with same name, "
			"or from the objects with names different from the name of the module")
		endif(is_objects_contain_name GREATER -1)
		set(is_build_simple "FALSE")
	endif(obj_sources_noext_rel STREQUAL ${name})

    # Create kbuild file
    set(kbuild_file "${CMAKE_CURRENT_BINARY_DIR}/Kbuild")
    FILE(WRITE "${kbuild_file}")

	# Build kbuild file - compiler flags
    if(kbuild_cflags OR kbuild_include_dirs)
		set(cflags_string)
		# Common flags
		if(kbuild_cflags)
			foreach(cflag ${kbuild_cflags})
				set(cflags_string "${cflags_string} ${cflag}")
			endforeach(cflag ${kbuild_cflags})
		endif(kbuild_cflags)

		# Include directories
		if(kbuild_include_dirs)
			foreach(dir ${kbuild_include_dirs})
				set(cflags_string "${cflags_string} -I${dir}")
			endforeach(dir ${kmodule_include_dirs})
		endif(kbuild_include_dirs)
		FILE(APPEND "${kbuild_file}" "ccflags-y := ${cflags_string}\n")
    endif(kbuild_cflags OR kbuild_include_dirs)

	# Build kbuild file - module string
	FILE(APPEND "${kbuild_file}" "obj-m := ${name}.o\n")

	# Build kbuild file - object sources string
	if(NOT is_build_simple)
		set(obj_src_string)
		foreach(obj ${c_sources_noext_rel} ${asm_sources_noext_rel} ${shipped_sources_noext_rel})
			set(obj_src_string "${obj_src_string} ${obj}.o")
		endforeach(obj ${c_sources_noext_rel} ${asm_sources_noext_rel}  ${shipped_sources_noext_rel})
		FILE(APPEND "${kbuild_file}" "${name}-y := ${obj_src_string}\n")
	endif(NOT is_build_simple)
	
    # Additional command and dependencies if module use
    # symbols from other modules
	if(kbuild_symbol_files)
		set(symvers_command COMMAND cat ${kbuild_symbol_files} >> ${symvers_file})
		list(APPEND ${depend_files} ${kbuild_symbol_files})
	endif(kbuild_symbol_files)
	# Create .cmd files for 'shipped' sources - gcc does not create them
	# automatically for some reason
	if(shipped_sources_noext_abs)
		set(cmd_create_command)
		foreach(shipped_source_noext_abs ${shipped_source_noext_abs})
			# Parse filename to dir and name (without extension)
			file_components(${shipped_source_noext_abs} DIR _dir NOTDIR _name)
			list(APPEND cmd_create_command
				COMMAND printf "cmd_%s.o := cp -p %s.o_shipped %s.o\\n"
					"${shipped_source_noext_abs}"
					"${shipped_source_noext_abs}"
					"${shipped_source_noext_abs}"
					> "${_dir}.${_name}.o.cmd")
		endforeach(shipped_source_noext_abs ${shipped_source_noext_abs})
	endif(shipped_sources_noext_abs)
	
	# The rule to create module
	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${name}.ko ${symvers_file}
		${cmd_create_command}
		${symvers_command}
		COMMAND $(MAKE) ${kedr_sparse_check} 
			ARCH=${KEDR_ARCH} CROSS_COMPILE=${KEDR_CROSS_COMPILE} 
			-C ${KBUILD_BUILD_DIR} M=${CMAKE_CURRENT_BINARY_DIR} modules
		DEPENDS ${depend_files}
	)
	# The rule to clean files
	_kbuild_module_clean_files(${name}
		C_SOURCE ${c_sources_noext_abs}
		ASM_SOURCE ${asm_sources_noext_abs}
		SHIPPED_SOURCE ${shipped_sources_noext_abs})
endfunction(kbuild_add_module name)

# kbuild_include_directories(dir1 .. dirn)
macro(kbuild_include_directories)
	list(APPEND kbuild_include_dirs ${ARGN})
endmacro(kbuild_include_directories)

# kbuild_use_symbols(symvers_file1 .. symvers_filen)
macro(kbuild_use_symbols)
	list(APPEND kbuild_symbol_files ${ARGN})
endmacro(kbuild_use_symbols)

# kbuild_add_dependencies(kmodule1 .. kmoduleN)
macro(kbuild_add_dependencies)
	list(APPEND kbuild_dependencies_modules ${ARGN})
endmacro(kbuild_add_dependencies)


# kbuild_add_definitions (flag1 ... flagN)
# Specify additional compiler flags for the module.
macro(kbuild_add_definitions)
	list(APPEND kbuild_cflags ${ARGN})
endmacro(kbuild_add_definitions)

# Internal functions

# _kbuild_module_clean_files(module_name
# 	[C_SOURCE c_source_noext_abs ...]
# 	[ASM_SOURCE asm_source_noext_abs ...]
#	[SHIPPED_SOURCE shipped_source_noext_abs ...])
#
# Tell CMake that intermediate files, created by kbuild system,
# should be cleaned with 'make clean'.

function(_kbuild_module_clean_files module_name)
	# List common files (names only) for cleaning
	set(common_files_names
		"built-in.o"
		".built-in.o.cmd"
		"Module.markers")
	# List module name-depending files (extensions only) for cleaning
	set(name_files_ext
		".o"
		".mod.c"
		".mod.o")
	# Same but for the files with names starting with a dot ('.').
	set(name_files_dot_ext
		".ko.cmd"
		".mod.o.cmd"
		".o.cmd")
	# List source name-depending files (extensions only) for cleaning
	set(source_name_files_ext
		".o")
	# Same but for the files with names starting with a dot ('.')
	set(source_name_files_dot_ext
		".o.cmd"
		".o.d")
	
	# Now collect all sort of files into list
	set(files_list)

	foreach(name ${common_files_names})
		list(APPEND files_list "${CMAKE_CURRENT_BINARY_DIR}/${name}")
	endforeach(name ${common_files_names})
	
	foreach(ext ${name_files_ext})
		list(APPEND files_list
			"${CMAKE_CURRENT_BINARY_DIR}/${module_name}${ext}")
	endforeach(ext ${name_files_ext})
	
	foreach(ext ${name_files_dot_ext})
		list(APPEND files_list
			"${CMAKE_CURRENT_BINARY_DIR}/.${module_name}${ext}")
	endforeach(ext ${name_files_ext})
	
	# State variable for parse argument list
	set(state "None")
	
	foreach(arg ${ARGN})
		if(arg STREQUAL "C_SOURCE")
			set(state "C")
		elseif(arg STREQUAL "ASM_SOURCE")
			set(state "ASM")
		elseif(arg STREQUAL "SHIPPED_SOURCE")
			set(state "SHIPPED")
		elseif(state STREQUAL "C" OR state STREQUAL "ASM" OR state STREQUAL "SHIPPED")
			# All the types of sources are processed in a similar way
			# Parse the filename and extract dir and name (without extension)
			file_components(${arg} DIR dir NOTDIR name)
			foreach(ext ${source_name_files_ext})
				list(APPEND files_list "${dir}${name}${ext}")
			endforeach(ext ${source_name_files_ext})
			foreach(ext ${source_name_files_dot_ext})
				list(APPEND files_list "${dir}.${name}${ext}")
			endforeach(ext ${source_name_files_ext})
		else(arg STREQUAL "C_SOURCE")
			message(FATAL "Unexpected argument: ${arg}")
		endif(arg STREQUAL "C_SOURCE")
	endforeach(arg ${ARGN})
	# Tell CMake that given files should be cleaned.
	set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${files_list}")
endfunction(_kbuild_module_clean_files module_name)
