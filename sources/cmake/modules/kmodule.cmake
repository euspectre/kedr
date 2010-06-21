set(kmodule_this_module_dir "${CMAKE_SOURCE_DIR}/cmake/modules/")
# kmodule_try_compile(RESULT_VAR bindir srcfile
#           [COMPILE_DEFINITIONS flags]
#           [OUTPUT_VARIABLE var])
# to be implemented...

# Similar to try_module in simplified form, but compile srcfile as
# kernel module, instead of user space program.


# kmodule_try_compile_wout(RESULT_VAR bindir srcfile out_var)
# is equivalent to 
# kmodule_try_compile(RESULT_VAR bindir srcfile OUTPUT_VARIABLE out_var)
function(kmodule_try_compile_wout RESULT_VAR bindir srcfile
    out_var)

try_compile(${RESULT_VAR} ${bindir}
                ${kmodule_this_module_dir}/kmodule_files
                kmodule_try_compile_target
                CMAKE_FLAGS "-DSRC_FILE:PATH=${srcfile}"
                OUTPUT_VARIABLE ${out_var})

endfunction(kmodule_try_compile_wout RESULT_VAR bindir srcfile out_var)

# kmodule_try_compile_wout_wcflags(RESULT_VAR bindir srcfile out_var cflags)
# is equivalent to 
# kmodule_try_compile(RESULT_VAR bindir srcfile COMPILE_DEFINITIONS cflags OUTPUT_VARIABLE out_var)
function(kmodule_try_compile_wout_wcflags RESULT_VAR bindir srcfile
    out_var my_cflags )

try_compile(${RESULT_VAR} ${bindir}
                ${kmodule_this_module_dir}/kmodule_files
                kmodule_try_compile_target
                CMAKE_FLAGS "-DSRC_FILE:PATH=${srcfile}" "-DCFLAGS:STRING=${my_cflags}"
                OUTPUT_VARIABLE ${out_var})

endfunction(kmodule_try_compile_wout_wcflags RESULT_VAR bindir srcfile
    out_var my_cflags)

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
            COMMAND perl ${kmodule_this_module_dir}/kmodule_files/scripts/look_kernel_function.pl ${function_name}
            RESULT_VARIABLE kmodule_is_function_exist_result
            OUTPUT_QUIET)
        if (kmodule_is_function_exist_result EQUAL 0)
            set(${RESULT_VAR} "TRUE" CACHE INTERNAL "Whether ${function_name} exist in the kernel")
        else(kmodule_is_function_exist_result EQUAL 0)
            set(${RESULT_VAR} "FALSE" CACHE INTERNAL "Whether ${function_name} exist in the kernel")
        endif(kmodule_is_function_exist_result EQUAL 0)
    endif(DEFINED ${RESULT_VAR})
    if (${RESULT_VAR})
        message(STATUS "${kmodule_is_function_exist_message} - found")
    else(${RESULT_VAR})
        message(STATUS "${kmodule_is_function_exist_message} - not found")
    endif(${RESULT_VAR})

endmacro(kmodule_is_function_exist function_name RESULT_VAR)
#kmodule_configure_kernel_functions(output_list func1 item1 [func2 item2...])
macro(kmodule_configure_kernel_functions output_list)
	set(${output_list})
	set(kmodule_input_lists ${ARGN})
	while(kmodule_input_lists)
		list(GET kmodule_input_lists 0 kmodule_func)
		set(kmodule_func_varname _KMODULE_IS_${kmodule_func}_EXIST)
        kmodule_is_function_exist(${kmodule_func} ${kmodule_func_varname})
		if(${kmodule_func_varname})
			list(GET kmodule_input_lists 1 kmodule_item)
			list(APPEND ${output_list} ${kmodule_item})
		endif(${kmodule_func_varname})
		list(REMOVE_AT kmodule_input_lists 0 1)
	endwhile(kmodule_input_lists)
endmacro(kmodule_configure_kernel_functions output_list)