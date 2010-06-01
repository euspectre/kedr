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
                ${CMAKE_SOURCE_DIR}/cmake_modules/kmodule_files
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
                ${CMAKE_SOURCE_DIR}/cmake_modules/kmodule_files
                kmodule_try_compile_target
                CMAKE_FLAGS "-DSRC_FILE:PATH=${srcfile}" "-DCFLAGS:STRING=${my_cflags}"
                OUTPUT_VARIABLE ${out_var})

endfunction(kmodule_try_compile_wout_wcflags RESULT_VAR bindir srcfile
    out_var my_cflags)

# kmodule_is_function_exist(function_name RESULT_VAR)
# Verify, whether given function exist in the kernel space on the current system.
# RESULT_VAR is TRUE, if function_name exist in the kernel space, FALSE otherwise.
macro(kmodule_is_function_exist function_name RESULT_VAR)

    if("${RESULT_VAR}" MATCHES "^${RESULT_VAR}$")
        message(STATUS "Looking for ${function_name} in the kernel")
        execute_process(
            COMMAND ${CMAKE_SOURCE_DIR}/cmake_modules/kmodule_files/scripts/look_kernel_function.pl ${function_name}
            RESULT_VARIABLE ${RESULT_VAR}
            OUTPUT_QUIET)
        if (${RESULT_VAR} EQUAL 0)
            message(STATUS "Looking for ${function_name} in the kernel - found")
            set(${RESULT_VAR} "TRUE")
        else(${RESULT_VAR} EQUAL 0)
            message(STATUS "Looking for ${function_name} in the kernel - not found")
            set(${RESULT_VAR} "FALSE")
        endif(${RESULT_VAR} EQUAL 0)
    endif("${RESULT_VAR}" MATCHES "^${RESULT_VAR}$")

endmacro(kmodule_is_function_exist function_name RESULT_VAR)