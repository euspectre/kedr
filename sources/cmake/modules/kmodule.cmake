set(kmodule_this_module_dir "${CMAKE_SOURCE_DIR}/cmake/modules/")

# kmodule_try_compile(RESULT_VAR bindir srcfile
#           [COMPILE_DEFINITIONS flags]
#           [OUTPUT_VARIABLE var])

# Similar to try_module in simplified form, but compile srcfile as
# kernel module, instead of user space program.

function(kmodule_try_compile RESULT_VAR bindir srcfile)
	set(is_compile_definitions_current "FALSE")
	set(is_output_var_current "FALSE")
	to_abs_path(src_abs_path "${srcfile}")
	foreach(arg ${ARGN})
		if(arg STREQUAL "COMPILE_DEFINITIONS")
			set(is_compile_definitions_current "TRUE")
			set(is_output_var_current "FALSE")
		elseif(arg STREQUAL "OUTPUT_VARIABLE")
			set(is_compile_definitions_current "FALSE")
			set(is_output_var_current "TRUE")
		elseif(is_compile_definitions_current)
			set(kmodule_cflags "${kmodule_cflags} ${arg}")
		elseif(is_output_var_current)
			set(output_variable "${arg}")
		else(arg STREQUAL "COMPILE_DEFINITIONS")
			message(FATAL_ERROR "Unknown parameter to kmodule_try_compile: '${arg}'.")
		endif(arg STREQUAL "COMPILE_DEFINITIONS")
	endforeach(arg ${ARGN})
	set(cmake_params "-DSRC_FILE:path=${src_abs_path}")
	if(DEFINED kmodule_cflags)
		list(APPEND cmake_params "-Dkmodule_flags=${kmodule_cflags}")
	endif(DEFINED kmodule_cflags)

	if(DEFINED output_variable)
		try_compile(result_tmp "${bindir}"
                "${kmodule_this_module_dir}/kmodule_files"
				"kmodule_try_compile_target"
                CMAKE_FLAGS ${cmake_params}
                OUTPUT_VARIABLE output_tmp)
		set("${output_variable}" "${output_tmp}" PARENT_SCOPE)
	else(DEFINED output_variable)
		try_compile("${RESULT_VAR}" "${bindir}"
                "${kmodule_this_module_dir}/kmodule_files"
				"kmodule_try_compile_target"
                CMAKE_FLAGS ${cmake_params})
	endif(DEFINED output_variable)
	set("${RESULT_VAR}" "${result_tmp}" PARENT_SCOPE)
endfunction(kmodule_try_compile RESULT_VAR bindir srcfile)

# prepare_function_lookup() prepares everything needed to check if 
# a function exists in the kernel by a more reliable (and slower) way
# than just checking /proc/kallsyms.
# Should be called at the top level, before payload modules are processed.
macro(prepare_function_lookup)
	message (STATUS "Preparing function lookup system")
	execute_process(
	    COMMAND sh ${kmodule_this_module_dir}/kmodule_files/scripts/prepare_function_lookup.sh ${CMAKE_SOURCE_DIR}
        RESULT_VARIABLE prepare_function_lookup_result
	    OUTPUT_VARIABLE prepare_function_lookup_output
		ERROR_VARIABLE prepare_function_lookup_output)
	if (NOT prepare_function_lookup_result EQUAL 0)
	    message ("Failed to prepare function lookup system.")
    	message ("Script output:\n${prepare_function_lookup_output}")
		message (FATAL_ERROR "Unable to prepare function lookup, aborting.")
	endif ()
	message (STATUS "Preparing function lookup system - done")
endmacro(prepare_function_lookup)

# kmodule_is_function_exist_impl(function_name RESULT_VAR)
# Verify, whether given function exist in the kernel space on the current system.
# RESULT_VAR is TRUE, if function_name exist in the kernel space, FALSE otherwise.
# RESULT_VAR is cached.
#
# The script that actually checks the existence of the function is chosen
# by the wrappers: 
# kmodule_configure_kernel_functions() and kmodule_configure_kernel_functions_hard().
macro(kmodule_is_function_exist_impl function_name RESULT_VAR)
    set(kmodule_is_function_exist_message "Looking for ${function_name} in the kernel")
	if (NOT DEFINED kmodule_is_function_exist_script)
		message(FATAL_ERROR "The script checking the existence of the functions is not specified")
	endif (NOT DEFINED kmodule_is_function_exist_script)

    if(DEFINED ${RESULT_VAR})
        set(kmodule_is_function_exist_message "${kmodule_is_function_exist_message} [cached]")
    else(DEFINED ${RESULT_VAR})
        execute_process(
            COMMAND sh ${kmodule_is_function_exist_script} ${function_name}
			WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            RESULT_VARIABLE kmodule_is_function_exist_result
            OUTPUT_QUIET)
        if (kmodule_is_function_exist_result EQUAL 0)
            set(${RESULT_VAR} "TRUE" CACHE INTERNAL "Whether ${function_name} exist in the kernel")
        elseif(kmodule_is_function_exist_result EQUAL 1)
            set(${RESULT_VAR} "FALSE" CACHE INTERNAL "Whether ${function_name} exist in the kernel")
        else(kmodule_is_function_exist_result EQUAL 0)
            message(FATAL_ERROR "Cannot determine whether function '${function_name}' exist in the kernel")
        endif(kmodule_is_function_exist_result EQUAL 0)
    endif(DEFINED ${RESULT_VAR})
    if (${RESULT_VAR})
        message(STATUS "${kmodule_is_function_exist_message} - found")
    else(${RESULT_VAR})
        message(STATUS "${kmodule_is_function_exist_message} - not found")
    endif(${RESULT_VAR})
endmacro(kmodule_is_function_exist_impl function_name RESULT_VAR)

# This macro creates the list of functions that actually exist on the 
# current system. This macro should not be used directly but rather via
# kmodule_configure_kernel_functions() or 
# kmodule_configure_kernel_functions_hard() - see below. These wrappers 
# differ only in how the presence and usability of the functions is 
# checked. *_hard variant is more reliable but can be significantly slower
# than kmodule_configure_kernel_functions().
#
# kmodule_configure_kernel_functions_impl(output_list 
#	{[REQUIRED | OPTIONAL] {func | ONE_OF_LIST}} ...)
#
# ONE_OF_LIST := ONE_OF_BEGIN {func ...} ONE_OF_END
#
# There are 2 modes of function lookup: 
# OPTIONAL - if the function doesn't exist, it is silently ignored.
# REQUIRED - if the function doesn't exist, FATAL_ERROR message is printed.
#
# Initial mode is REQUIRED, and it can be changed at any time by REQUIRED 
# and OPTIONAL keywords.
#
# ONE_OF_BEGIN/ONE_OF_END determine a section for which no more than one 
# function among all listed there should exist. FATAL_ERROR message is 
# printed otherwise. When mode is REQUIRED, precisely one function must 
# exist.
# Inside this section other keywords must not be used (even another 
# ONE_OF_BEGIN).

macro(kmodule_configure_kernel_functions_impl output_list)
	set(kmodule_configure_kernel_functions_mode "REQUIRED")
	set(kmodule_configure_kernel_functions_one_of_section "FALSE")
	set(${output_list})
	foreach(arg ${ARGN})
		if(arg STREQUAL "REQUIRED" OR arg STREQUAL "OPTIONAL")
			if(kmodule_configure_kernel_functions_one_of_section)
				message(FATAL_ERROR "Inside ONE_OF_BEGIN/ONE_OF_END section other keywords are not allowed.")
			endif(kmodule_configure_kernel_functions_one_of_section)
			set(kmodule_configure_kernel_functions_mode ${arg})
		elseif(arg STREQUAL "ONE_OF_BEGIN")
			if(kmodule_configure_kernel_functions_one_of_section)
				message(FATAL_ERROR "Nested ONE_OF_BEGIN/ONE_OF_END sections are not allowed.")
			endif(kmodule_configure_kernel_functions_one_of_section)
			set(kmodule_configure_kernel_functions_one_of_section "TRUE")
			set(kmodule_configure_kernel_functions_one_of_section_function)
		elseif(arg STREQUAL "ONE_OF_END")
			if(NOT kmodule_configure_kernel_functions_one_of_section)
				message(FATAL_ERROR "ONE_OF_END without ONE_OF_BEGIN is not allowed.")
			endif(NOT kmodule_configure_kernel_functions_one_of_section)
			if(kmodule_configure_kernel_functions_one_of_section_function)
				list(APPEND ${output_list} ${kmodule_configure_kernel_functions_one_of_section_function})				
			else(kmodule_configure_kernel_functions_one_of_section_function)
				if(kmodule_configure_kernel_functions_mode STREQUAL "REQUIRED")
					message(FATAL_ERROR "No any function in ONE_OF section exists in the kernel, but it is required.")
				endif(kmodule_configure_kernel_functions_mode STREQUAL "REQUIRED")
			endif(kmodule_configure_kernel_functions_one_of_section_function)
			set(kmodule_configure_kernel_functions_one_of_section "FALSE")
		else(arg STREQUAL "REQUIRED" OR arg STREQUAL "OPTIONAL")
			set(kmodule_func_varname _KMODULE_IS_${arg}_EXIST)
			kmodule_is_function_exist_impl(${arg} ${kmodule_func_varname})
			if(kmodule_configure_kernel_functions_one_of_section)
				if(${kmodule_func_varname})
					if(kmodule_configure_kernel_functions_one_of_section_function)
						message(FATAL_ERROR "Two functions from ONE_OF sections exist in the kernel.")
					else(kmodule_configure_kernel_functions_one_of_section_function)
						set(kmodule_configure_kernel_functions_one_of_section_function ${arg})	
					endif(kmodule_configure_kernel_functions_one_of_section_function)
				endif(${kmodule_func_varname})
			else(kmodule_configure_kernel_functions_one_of_section)
				if(${kmodule_func_varname})
					list(APPEND ${output_list} ${arg})
				else(${kmodule_func_varname})
					if(kmodule_configure_kernel_functions_mode STREQUAL "REQUIRED")
						message(FATAL_ERROR "Function ${arg} is absent in the kernel, but it is required.")
					endif(kmodule_configure_kernel_functions_mode STREQUAL "REQUIRED")
				endif(${kmodule_func_varname})
			endif(kmodule_configure_kernel_functions_one_of_section)
		endif(arg STREQUAL "REQUIRED" OR arg STREQUAL "OPTIONAL")
	endforeach(arg ${ARGN})
	if(kmodule_configure_kernel_functions_one_of_section)
		message(FATAL_ERROR "Unclosed ONE_OF_SECTION (ONE_OF_BEGIN without ONE_OF_END)")
	endif(kmodule_configure_kernel_functions_one_of_section)
endmacro(kmodule_configure_kernel_functions_impl output_list)

# A version of the macro that uses weaker checks to see if the functions
# exist in the kernel (default).
macro(kmodule_configure_kernel_functions output_list)
	set (kmodule_is_function_exist_script 
"${kmodule_this_module_dir}/kmodule_files/scripts/lookup_kernel_function.sh")
	kmodule_configure_kernel_functions_impl(${output_list} ${ARGN})
endmacro(kmodule_configure_kernel_functions output_list)

# A version of the macro that uses reliable (but sometimes significantly 
# slower) checks to see if the functions exist in the kernel.
macro(kmodule_configure_kernel_functions_hard output_list)
	set (kmodule_is_function_exist_script 
"${kmodule_this_module_dir}/kmodule_files/scripts/lookup_kernel_function_hard.sh")
	kmodule_configure_kernel_functions_impl(${output_list} ${ARGN})
endmacro(kmodule_configure_kernel_functions_hard output_list)
