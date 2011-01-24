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

# kmodule_is_function_exist(function_name RESULT_VAR)
# Verify, whether given function exist in the kernel space on the current system.
# RESULT_VAR is TRUE, if function_name exist in the kernel space, FALSE otherwise.
# RESULT_VAR is cached.

macro(kmodule_is_function_exist function_name RESULT_VAR)

    set(kmodule_is_function_exist_message "Looking for ${function_name} in the kernel")
    if(DEFINED ${RESULT_VAR})
        set(kmodule_is_function_exist_message "${kmodule_is_function_exist_message} [cached]")
    else(DEFINED ${RESULT_VAR})
        execute_process(
            COMMAND sh ${kmodule_this_module_dir}/kmodule_files/scripts/look_kernel_function.sh ${function_name}
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

endmacro(kmodule_is_function_exist function_name RESULT_VAR)

# Build list of functions, which really exist on the current system
#
# kmodule_configure_kernel_functions(output_list {[REQUIRED | OPTIONAL] {func | ONE_OF_LIST}} ...)
#
# ONE_OF_LIST := ONE_OF_BEGIN {func ...} ONE_OF_END
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

macro(kmodule_configure_kernel_functions output_list)
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
			kmodule_is_function_exist(${arg} ${kmodule_func_varname})
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
endmacro(kmodule_configure_kernel_functions output_list)