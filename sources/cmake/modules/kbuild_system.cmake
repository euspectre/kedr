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
# Only one call of kbuild_add_module or kbuild_add_objects
# is allowed in the CMakeLists.txt.
#
# In case when 'sources' omitted, module will be built 
# from "${name}.c" source.

function(kbuild_add_module name)
    set(symvers_file ${CMAKE_CURRENT_BINARY_DIR}/Module.symvers)
	#Global target
	add_custom_target(${name} ALL
			DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${name}.ko ${symvers_file})
	if(kbuild_dependencies_modules)
		add_dependencies(${name} ${kbuild_dependencies_modules})
	endif(kbuild_dependencies_modules)
	#Sources
	if(ARGN)
		set(sources ${ARGN})
	else(ARGN)
		set(sources "${CMAKE_CURRENT_BINARY_DIR}/${name}.c")
	endif(ARGN)
	#Sources with absolute paths
	to_abs_path(sources_abs ${sources})
	#list of files from which module building is depended
	set(depend_files)
	#Sources of "c" type, but without extension
	#(for clean files, 
	#for out-of-source builds do not create files in source tree)
	set(c_sources_noext_abs)
	#Sources of "o" type, but without extension
	set(o_sources_noext_abs)
	foreach(c_source_noext_abs ${c_sources_noext_abs})
		_kbuild_add_clean_files_c(${c_source_noext_abs} clean_files_list)
		list(APPEND clean_files_list "${c_source_noext_abs}.o")
	endforeach(c_source_noext_abs ${c_sources_noext_abs})
	#sort sources
	foreach(source_abs ${sources_abs})
		string(REGEX MATCH "(.+)((\\.c)|(\\.o))$" is_obj_source ${source_abs})
		if(is_obj_source)
			#real sources
			set(obj_source_noext_abs ${CMAKE_MATCH_1})
			if(CMAKE_MATCH_2 STREQUAL ".c")
				is_path_inside_dir(is_in_source ${CMAKE_SOURCE_DIR} ${source_abs})
				is_path_inside_dir(is_in_binary ${CMAKE_BINARY_DIR} ${source_abs})
				if(is_in_source AND NOT is_in_binary)
					#special process c-sources in source tree
					file(RELATIVE_PATH c_source_rel ${CMAKE_SOURCE_DIR} ${source_abs})
					set(c_source_abs_real ${CMAKE_BINARY_DIR}/${c_source_rel})
					#add rule for create duplicate..
					rule_copy_file(${c_source_abs_real} ${source_abs})
					#..and forgot initial file
					set(source_abs ${c_source_abs_real})
					#regenerate source without extension
					string(REGEX REPLACE "(.+)\\.c" "\\1" 
						obj_source_noext_abs
						${source_abs})
				endif(is_in_source AND NOT is_in_binary)
				list(APPEND c_sources_noext_abs ${obj_source_noext_abs})
			else(CMAKE_MATCH_2 STREQUAL ".c")
				list(APPEND o_sources_noext_abs ${obj_source_noext_abs})
			endif(CMAKE_MATCH_2 STREQUAL ".c")
		else(is_obj_source)
			#sources only for DEPENDS
		endif(is_obj_source)
		list(APPEND depend_files ${source_abs})
	endforeach(source_abs ${sources_abs})
	#Object sources relative to current dir
	#(for $(module)-y :=)
	set(obj_sources_noext_rel)
	foreach(obj_sources_noext_abs
			${c_sources_noext_abs} ${o_sources_noext_abs})
		file(RELATIVE_PATH obj_source_noext_rel
			${CMAKE_CURRENT_BINARY_DIR} ${obj_sources_noext_abs})
		list(APPEND obj_sources_noext_rel ${obj_source_noext_rel})
	endforeach(obj_sources_noext_abs)

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
			message(FATAL_ERROR "Module should be built "
			"either from only one object with same name, "
			"or from objects with names different from the name of the module")
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

	# Build kbuild file - compiler flags
	set(cflags_string "ccflags-y := ")
    if(kbuild_cflags)
		foreach(cflag ${kbuild_cflags})
       		set(cflags_string "${cflags_string} ${cflag}")
		endforeach(cflag ${kbuild_cflags})
	endif(kbuild_cflags)

	# compiler flags - directories
	if(kbuild_include_dirs)
		foreach(dir ${kbuild_include_dirs})
			set(cflags_string "${cflags_string} -I${dir}")
		endforeach(dir ${kmodule_include_dirs})
	endif(kbuild_include_dirs)
    
    # Configure kbuild file
	configure_file(${kbuild_this_module_dir}/kbuild_system_files/Kbuild.in
					${CMAKE_CURRENT_BINARY_DIR}/Kbuild
					)
	
    # Create rules, depending on kbuild_use_symbols call
	if(kbuild_symbol_files)
    	add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${name}.ko ${symvers_file}
				COMMAND cat ${kbuild_symbol_files} >> ${symvers_file}
    			COMMAND $(MAKE) ARCH=${KEDR_ARCH} CROSS_COMPILE=${KEDR_CROSS_COMPILE} 
					-C ${KBUILD_BUILD_DIR} M=${CMAKE_CURRENT_BINARY_DIR} modules
    			DEPENDS ${depend_files} ${kbuild_symbol_files}
                )
	else(kbuild_symbol_files)
    	add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${name}.ko ${symvers_file}
    			COMMAND $(MAKE) ARCH=${KEDR_ARCH} CROSS_COMPILE=${KEDR_CROSS_COMPILE}
					-C ${KBUILD_BUILD_DIR} M=${CMAKE_CURRENT_BINARY_DIR} modules
    			DEPENDS ${depend_files}
    			)
	endif(kbuild_symbol_files)

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
			COMMAND $(MAKE) ARCH=${KEDR_ARCH} CROSS_COMPILE=${KEDR_CROSS_COMPILE}
				-C ${KBUILD_BUILD_DIR} M=${CMAKE_CURRENT_BINARY_DIR}
			DEPENDS ${depend_files}
			)

	set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${clean_files_list}")
endfunction(kbuild_add_object source)

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
