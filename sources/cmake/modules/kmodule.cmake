set(kmodule_this_module_dir "${CMAKE_SOURCE_DIR}/cmake/modules/")
set(kmodule_test_sources_dir "${CMAKE_SOURCE_DIR}/cmake/kmodule_sources")

set(kmodule_function_map_file "")
if (CMAKE_CROSSCOMPILING)
	if (KEDR_SYSTEM_MAP_FILE)
		set (kmodule_function_map_file "${KEDR_SYSTEM_MAP_FILE}")
	else (KEDR_SYSTEM_MAP_FILE)
# KEDR_SYSTEM_MAP_FILE is not specified, construct the default path 
# to the symbol map file.
		set (kmodule_function_map_file 
	"${KEDR_ROOT_DIR}/boot/System.map-${KBUILD_VERSION_STRING}"
		)
	endif (KEDR_SYSTEM_MAP_FILE)
endif (CMAKE_CROSSCOMPILING)

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
			message(FATAL_ERROR 
				"Unknown parameter passed to kmodule_try_compile: '${arg}'."
			)
		endif(arg STREQUAL "COMPILE_DEFINITIONS")
	endforeach(arg ${ARGN})
	set(cmake_params 
		"-DSRC_FILE:path=${src_abs_path}" 
		"-DKERNELDIR=${KBUILD_BUILD_DIR}"
		"-DKEDR_ARCH=${KEDR_ARCH}"
		"-DKEDR_CROSS_COMPILE=${KEDR_CROSS_COMPILE}"
	)
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
		try_compile(result_tmp "${bindir}"
                "${kmodule_this_module_dir}/kmodule_files"
				"kmodule_try_compile_target"
                CMAKE_FLAGS ${cmake_params})
	endif(DEFINED output_variable)
	set("${RESULT_VAR}" "${result_tmp}" PARENT_SCOPE)
endfunction(kmodule_try_compile RESULT_VAR bindir srcfile)

# List of unreliable functions,
# which existance in system map doesn't prove existance in headers.
set(unreliable_functions_list
    "__kmalloc_node"
	"kmem_cache_alloc_node"
	"kmem_cache_alloc_node_notrace"
	"kmem_cache_alloc_node_trace"
)

# kmodule_is_function_exist(function_name RESULT_VAR)
# Verify, whether given function exist in the kernel space on the current system.
# RESULT_VAR is TRUE, if function_name exist in the kernel space, FALSE otherwise.
# RESULT_VAR is cached.

function(kmodule_is_function_exist function_name RESULT_VAR)
    set(kmodule_is_function_exist_message "Looking for ${function_name} in the kernel")

    if(DEFINED ${RESULT_VAR})
        set(kmodule_is_function_exist_message "${kmodule_is_function_exist_message} [cached]")
    else(DEFINED ${RESULT_VAR})
        execute_process(
            COMMAND sh "${kmodule_this_module_dir}/kmodule_files/scripts/lookup_kernel_function.sh"
				${function_name} ${kmodule_function_map_file}
			WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            RESULT_VARIABLE kmodule_is_function_exist_result
            OUTPUT_QUIET)

		if (kmodule_is_function_exist_result EQUAL 0)
            list(FIND unreliable_functions_list ${function_name} unreliable_function_index)
            if(unreliable_function_index GREATER -1)
                # Additional verification for unreliable function
                kmodule_try_compile(kmodule_function_is_exist_reliable
                    "${CMAKE_BINARY_DIR}/check_unreliable_functions/${function_name}"
                    "${kmodule_test_sources_dir}/check_unreliable_functions/${function_name}.c"
                )
                if(NOT kmodule_function_is_exist_reliable)
                    set(kmodule_is_function_exist_result 1)
                endif(NOT kmodule_function_is_exist_reliable)
            endif(unreliable_function_index GREATER -1)
        endif(kmodule_is_function_exist_result EQUAL 0)

        if (kmodule_is_function_exist_result EQUAL 0)
            set(${RESULT_VAR} "TRUE" CACHE INTERNAL "Does ${function_name} exist in the kernel?")
        elseif(kmodule_is_function_exist_result EQUAL 1)
            set(${RESULT_VAR} "FALSE" CACHE INTERNAL "Does ${function_name} exist in the kernel?")
        else(kmodule_is_function_exist_result EQUAL 0)
            message(FATAL_ERROR 
"Cannot determine whether function '${function_name}' exists in the kernel"
			)
        endif(kmodule_is_function_exist_result EQUAL 0)
    endif(DEFINED ${RESULT_VAR})
    if (${RESULT_VAR})
        message(STATUS "${kmodule_is_function_exist_message} - found")
    else(${RESULT_VAR})
        message(STATUS "${kmodule_is_function_exist_message} - not found")
    endif(${RESULT_VAR})
endfunction(kmodule_is_function_exist function_name RESULT_VAR)

# Creates the list of functions that actually exist on the 
# current system.
#
# kmodule_configure_kernel_functions(output_list 
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

function(kmodule_configure_kernel_functions output_list)
	set(kmodule_configure_kernel_functions_mode "REQUIRED")
	set(kmodule_configure_kernel_functions_one_of_section "FALSE")
	set(output_list_tmp)
    set(${output_list})
	foreach(arg ${ARGN})
		if(arg STREQUAL "REQUIRED" OR arg STREQUAL "OPTIONAL")
			if(kmodule_configure_kernel_functions_one_of_section)
				message(FATAL_ERROR 
"Inside ONE_OF_BEGIN/ONE_OF_END section, other keywords are not allowed."
				)
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
				list(APPEND output_list_tmp ${kmodule_configure_kernel_functions_one_of_section_function})				
			else(kmodule_configure_kernel_functions_one_of_section_function)
				if(kmodule_configure_kernel_functions_mode STREQUAL "REQUIRED")
					message(FATAL_ERROR 
"None of the functions listed in ONE_OF section exist in the kernel but it is required."
					)
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
					list(APPEND output_list_tmp ${arg})
				else(${kmodule_func_varname})
					if(kmodule_configure_kernel_functions_mode STREQUAL "REQUIRED")
						message(FATAL_ERROR "Function ${arg} is not found in the kernel but it is required.")
					endif(kmodule_configure_kernel_functions_mode STREQUAL "REQUIRED")
				endif(${kmodule_func_varname})
			endif(kmodule_configure_kernel_functions_one_of_section)
		endif(arg STREQUAL "REQUIRED" OR arg STREQUAL "OPTIONAL")
	endforeach(arg ${ARGN})
	if(kmodule_configure_kernel_functions_one_of_section)
		message(FATAL_ERROR "Found ONE_OF_BEGIN without ONE_OF_END")
	endif(kmodule_configure_kernel_functions_one_of_section)
    set(${output_list} ${output_list_tmp} PARENT_SCOPE)
endfunction(kmodule_configure_kernel_functions output_list)

############################################################################
# Utility macros to check for particular features. If the particular feature
# is supported, the macros will set the corresponding variable to TRUE, 
# otherwise - to FALSE (the name of variable is mentioned in the comments 
# for the macro). 
############################################################################

# Check if the system has everything necessary to build at least simple
# kernel modules. 
# The macro sets variable 'MODULE_BUILD_SUPPORTED'.
macro(check_module_build)
	set(check_module_build_message 
		"Checking if kernel modules can be built on this system"
	)
	message(STATUS "${check_module_build_message}")
	if (DEFINED MODULE_BUILD_SUPPORTED)
		set(check_module_build_message 
"${check_module_build_message} [cached] - ${MODULE_BUILD_SUPPORTED}"
		)
	else (DEFINED MODULE_BUILD_SUPPORTED)
		kmodule_try_compile(module_build_supported_impl 
			"${CMAKE_BINARY_DIR}/check_module_build"
			"${kmodule_test_sources_dir}/check_module_build/module.c"
		)
		if (module_build_supported_impl)
			set(MODULE_BUILD_SUPPORTED "yes" CACHE INTERNAL
				"Can kernel modules be built on this system?"
			)
		else (module_build_supported_impl)
			set(MODULE_BUILD_SUPPORTED "no")
			message(FATAL_ERROR 
				"Kernel modules cannot be built on this system"
			)
		endif (module_build_supported_impl)
				
		set(check_module_build_message 
"${check_module_build_message} - ${MODULE_BUILD_SUPPORTED}"
		)
	endif (DEFINED MODULE_BUILD_SUPPORTED)
	message(STATUS "${check_module_build_message}")
endmacro(check_module_build)

# Check if the version of the kernel is acceptable
# The macro sets variable 'KERNEL_VERSION_OK'.
macro(check_kernel_version kversion_major kversion_minor kversion_micro)
	set(check_kernel_version_string 
"${kversion_major}.${kversion_minor}.${kversion_micro}"
	)
	set(check_kernel_version_message 
"Checking if the kernel version is ${check_kernel_version_string} or newer"
	)
	message(STATUS "${check_kernel_version_message}")
	if (DEFINED KERNEL_VERSION_OK)
		set(check_kernel_version_message 
"${check_kernel_version_message} [cached] - ${KERNEL_VERSION_OK}"
		)
	else (DEFINED KERNEL_VERSION_OK)
		string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" 
			real_kernel_version_string
			"${KBUILD_VERSION_STRING}"
		)

		if (real_kernel_version_string VERSION_LESS check_kernel_version_string)
			set(KERNEL_VERSION_OK "no")
			message(FATAL_ERROR 
"Kernel version is ${real_kernel_version_string} but ${check_kernel_version_string} or newer is required."
			)
		else ()
			set(KERNEL_VERSION_OK "yes" CACHE INTERNAL
				"Is kernel version high enough?"
			)
		endif ()
				
		set(check_kernel_version_message 
"${check_kernel_version_message} - ${KERNEL_VERSION_OK}"
		)
	endif (DEFINED KERNEL_VERSION_OK)
	message(STATUS "${check_kernel_version_message}")
endmacro(check_kernel_version kversion_major kversion_minor kversion_micro)

# Check if reliable stack trace information can be obtained. 
# This is the case, for example, if the kernel is compiled with support
# for frame pointers and/or stack unwind on.
# The macro sets variable 'STACK_TRACE_RELIABLE'.
macro(check_stack_trace)
	set(check_stack_trace_message 
		"Checking if stack trace information is reliable"
	)
	message(STATUS "${check_stack_trace_message}")
	if (DEFINED STACK_TRACE_RELIABLE)
		set(check_stack_trace_message 
"${check_stack_trace_message} [cached] - ${STACK_TRACE_RELIABLE}"
		)
	else (DEFINED STACK_TRACE_RELIABLE)
		kmodule_try_compile(stack_trace_reliable_impl 
			"${CMAKE_BINARY_DIR}/check_stack_trace"
			"${kmodule_test_sources_dir}/check_stack_trace/module.c"
		)
		if (stack_trace_reliable_impl)
			set(STACK_TRACE_RELIABLE "yes" CACHE INTERNAL
				"Are stack traces reliable on this system?"
			)
		else (stack_trace_reliable_impl)
			set(STACK_TRACE_RELIABLE "no" CACHE INTERNAL
				"Are stack traces reliable on this system?"
			)
		endif (stack_trace_reliable_impl)
				
		set(check_stack_trace_message 
"${check_stack_trace_message} - ${STACK_TRACE_RELIABLE}"
		)
	endif (DEFINED STACK_TRACE_RELIABLE)
	message(STATUS "${check_stack_trace_message}")

	if (NOT STACK_TRACE_RELIABLE)
		message ("\n[WARNING]\n"
		"It looks like reliable stack traces cannot be obtained on this system.\n"
		"The output of KEDR-based tools like LeakCheck will be less detailed\n"
		"(each stack trace shown will contain only one frame).\n"
		"If this is not acceptable, you could rebuild the kernel with\n"
		"CONFIG_FRAME_POINTER or CONFIG_STACK_UNWIND (if available) set to \"y\"\n"
		"and then reconfigure and rebuild KEDR.\n")
	endif (NOT STACK_TRACE_RELIABLE)
endmacro(check_stack_trace)

# Check whether ring buffer is implemented by the kernel.
# Set cache variable RING_BUFFER_IMPLEMENTED according to this checking.
function(check_ring_buffer)
	set(check_ring_buffer_message 
		"Checking if ring buffer is implemented by the kernel"
	)
	message(STATUS "${check_ring_buffer_message}")
	if (DEFINED RING_BUFFER_IMPLEMENTED)
		set(check_ring_buffer_message 
"${check_ring_buffer_message} [cached] - ${RING_BUFFER_IMPLEMENTED}"
		)
	else (DEFINED RING_BUFFER_IMPLEMENTED)
		kmodule_try_compile(ring_buffer_implemented_impl 
			"${CMAKE_BINARY_DIR}/check_ring_buffer"
			"${kmodule_test_sources_dir}/check_ring_buffer/module.c"
		)
		if (ring_buffer_implemented_impl)
			set(RING_BUFFER_IMPLEMENTED "yes" CACHE INTERNAL
				"Whether ring buffer is implemented by the kernel"
			)
		else (ring_buffer_implemented_impl)
			set(RING_BUFFER_IMPLEMENTED "no" CACHE INTERNAL
				"Whether ring buffer is implemented by the kernel"
			)
		endif (ring_buffer_implemented_impl)
				
		set(check_ring_buffer_message 
"${check_ring_buffer_message} - ${RING_BUFFER_IMPLEMENTED}"
		)
	endif (DEFINED RING_BUFFER_IMPLEMENTED)
	message(STATUS "${check_ring_buffer_message}")
	
	if (NOT RING_BUFFER_IMPLEMENTED)
		message("\n[WARNING]\n Ring buffer is not supported by the system.\n"
			"This make tracing in KEDR unavailable, so call monitoring will also do not work."
			"If this is not acceptable, you could rebuild the kernel with\n"
			"CONFIG_RING_BUFFER set to \"y\" and then reconfigure and rebuild KEDR.\n")
	endif (NOT RING_BUFFER_IMPLEMENTED)

endfunction(check_ring_buffer)

# Check which memory allocator is used by the kernel.
# Set KERNEL_MEMORY_ALLOCATOR to 'slab', 'slub', 'slob' or 'other'.
#
# Some functions in that allocators may have same names, but different signatures.
function(check_allocator)
	set(check_allocator_message 
		"Checking which memory allocator is used by the kernel"
	)
		message(STATUS "${check_allocator_message}")
	if (DEFINED KERNEL_MEMORY_ALLOCATOR)
		set(check_allocator_message 
"${check_allocator_message} [cached] - ${KERNEL_MEMORY_ALLOCATOR}"
		)
	else (DEFINED KERNEL_MEMORY_ALLOCATOR)
		kmodule_try_compile(is_allocator_slab 
			"${CMAKE_BINARY_DIR}/check_allocator_slab"
			"${kmodule_test_sources_dir}/check_allocator/module.c"
			COMPILE_DEFINITIONS "-DIS_ALLOCATOR_SLAB"
		)
		if (is_allocator_slab)
			set(allocator "slab")
		else (is_allocator_slab)
			kmodule_try_compile(is_allocator_slub 
				"${CMAKE_BINARY_DIR}/check_allocator_slub"
				"${kmodule_test_sources_dir}/check_allocator/module.c"
				COMPILE_DEFINITIONS "-DIS_ALLOCATOR_SLUB"
			)
			if (is_allocator_slub)
				set(allocator "slub")
			else (is_allocator_slub)
				kmodule_try_compile(is_allocator_slob 
					"${CMAKE_BINARY_DIR}/check_allocator_slob"
					"${kmodule_test_sources_dir}/check_allocator/module.c"
					COMPILE_DEFINITIONS "-DIS_ALLOCATOR_SLOB"
				)
				if (is_allocator_slob)
					set(allocator "slob")
				else (is_allocator_slub)
					set(allocator "other")
				endif (is_allocator_slob)
			endif (is_allocator_slub)
		endif (is_allocator_slab)
		set(KERNEL_MEMORY_ALLOCATOR "${allocator}" CACHE INTERNAL
			"Memory allocator which is used by the kernel"
		)
				
		set(check_allocator_message 
"${check_allocator_message} - ${KERNEL_MEMORY_ALLOCATOR}"
		)
	endif (DEFINED KERNEL_MEMORY_ALLOCATOR)
	message(STATUS "${check_allocator_message}")

endfunction(check_allocator)

# Check if 'kfree_rcu' is available in the kernel (it is likely to be 
# a macro or an inline). If it is available, we should handle it as 
# 'free' in LeakCheck. As KEDR cannot normally intercept kfree_rcu()
# itself, it needs to intercept call_rcu/call_rcu_sched and check their
# arguments.
# The macro sets variable 'HAVE_KFREE_RCU'.
macro(check_kfree_rcu)
	set(check_kfree_rcu_message 
		"Checking if kfree_rcu() is available"
	)
	message(STATUS "${check_kfree_rcu_message}")
	if (DEFINED HAVE_KFREE_RCU)
		set(check_kfree_rcu_message 
"${check_kfree_rcu_message} [cached] - ${HAVE_KFREE_RCU}"
		)
	else (DEFINED HAVE_KFREE_RCU)
		kmodule_try_compile(have_kfree_rcu_impl 
			"${CMAKE_BINARY_DIR}/check_kfree_rcu"
			"${kmodule_test_sources_dir}/check_kfree_rcu/module.c"
		)
		if (have_kfree_rcu_impl)
			set(HAVE_KFREE_RCU "yes" CACHE INTERNAL
				"Is kfree_rcu() available?"
			)
		else (have_kfree_rcu_impl)
			set(HAVE_KFREE_RCU "no" CACHE INTERNAL
				"Is kfree_rcu() available?"
			)
		endif (have_kfree_rcu_impl)
				
		set(check_kfree_rcu_message 
"${check_kfree_rcu_message} - ${HAVE_KFREE_RCU}"
		)
	endif (DEFINED HAVE_KFREE_RCU)
	message(STATUS "${check_kfree_rcu_message}")
endmacro(check_kfree_rcu)
############################################################################
