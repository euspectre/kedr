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
# 			[OUTPUT_VARIABLE var]
#			[COPY_FILE filename])

# Similar to try_module in simplified form, but compile srcfile as
# kernel module, instead of user space program.

function(kmodule_try_compile RESULT_VAR bindir srcfile)
	to_abs_path(src_abs_path "${srcfile}")
	# State for parse argument list
	set(state "None")
	foreach(arg ${ARGN})
		if(arg STREQUAL "COMPILE_DEFINITIONS")
			set(state "COMPILE_DEFINITIONS")
		elseif(arg STREQUAL "OUTPUT_VARIABLE")
			set(state "OUTPUT_VARIABLE")
		elseif(arg STREQUAL "COPY_FILE")
			set(state "COPY_FILE")
		elseif(state STREQUAL "COMPILE_DEFINITIONS")
			set(kmodule_cflags "${kmodule_cflags} ${arg}")
		elseif(state STREQUAL "OUTPUT_VARIABLE")
			set(output_variable "${arg}")
			set(state "None")
		elseif(state STREQUAL "COPY_FILE")
			set(copy_file_variable "${arg}")
			set(state "None")
		else(arg STREQUAL "COMPILE_DEFINITIONS")
			message(FATAL_ERROR 
				"Unexpected parameter passed to kmodule_try_compile: '${arg}'."
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
	if(copy_file_variable)
		list(APPEND cmake_params "-DCOPY_FILE=${copy_file_variable}")
	endif(copy_file_variable)

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

# List of unreliable functions, that is, the functions that may be
# be exported and mentioned in System.map but still cannot be used
# because no header provides their declarations.
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
			kedr_find_function(${arg} ${kmodule_func_varname})
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
"There are problems with building kernel modules on this system. "
"Please check that the appropriate kernel headers and build tools "
"are installed."
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
# It also sets 'KERNEL_VERSION', which is the version of the kernel in the
# form "major.minor.micro".
macro(check_kernel_version kversion_major kversion_minor kversion_micro)
	set(check_kernel_version_string 
"${kversion_major}.${kversion_minor}.${kversion_micro}"
	)
	set(check_kernel_version_message 
"Checking if the kernel version is ${check_kernel_version_string} or newer"
	)
	message(STATUS "${check_kernel_version_message}")

	string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+"
		KERNEL_VERSION
		"${KBUILD_VERSION_STRING}"
	)
	
	if (DEFINED KERNEL_VERSION_OK)
		set(check_kernel_version_message 
"${check_kernel_version_message} [cached] - ${KERNEL_VERSION_OK}"
		)
	else (DEFINED KERNEL_VERSION_OK)
		if (KERNEL_VERSION VERSION_LESS check_kernel_version_string)
			set(KERNEL_VERSION_OK "no")
			message(FATAL_ERROR 
"Kernel version is ${KERNEL_VERSION} but ${check_kernel_version_string} or newer is required."
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
		"Checking if ring buffer is implemented in the kernel"
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
				"Whether ring buffer is implemented in the kernel"
			)
		else (ring_buffer_implemented_impl)
			set(RING_BUFFER_IMPLEMENTED "no" CACHE INTERNAL
				"Whether ring buffer is implemented in the kernel"
			)
		endif (ring_buffer_implemented_impl)
				
		set(check_ring_buffer_message 
"${check_ring_buffer_message} - ${RING_BUFFER_IMPLEMENTED}"
		)
	endif (DEFINED RING_BUFFER_IMPLEMENTED)
	message(STATUS "${check_ring_buffer_message}")
	
	if (NOT RING_BUFFER_IMPLEMENTED)
		message("\n[WARNING]\nRing buffer is not supported by the system.\n"
			"The tracing facilities as well as call monitoring plugins will not be built.\n"
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

# Check if posix_acl_from_xattr() accepts struct user_namespace as the
# first argument.
# The macro sets variable 'POSIX_ACL_XATTR_HAS_USER_NS'.
macro(check_xattr_user_ns)
	set(check_xattr_user_ns_message
"Checking if posix_acl_from_xattr() has struct user_namespace * argument"
	)
	message(STATUS "${check_xattr_user_ns_message}")
	if (DEFINED POSIX_ACL_XATTR_HAS_USER_NS)
		set(check_xattr_user_ns_message
"${check_xattr_user_ns_message} [cached] - ${POSIX_ACL_XATTR_HAS_USER_NS}"
		)
	else ()
		kmodule_try_compile(have_xattr_user_ns_impl
			"${CMAKE_BINARY_DIR}/check_xattr_user_ns"
			"${kmodule_test_sources_dir}/check_xattr_user_ns/module.c"
		)
		if (have_xattr_user_ns_impl)
			set(POSIX_ACL_XATTR_HAS_USER_NS "yes" CACHE INTERNAL
	"Does posix_acl_from_xattr() have struct user_namespace argument?"
			)
		else (have_xattr_user_ns_impl)
			set(POSIX_ACL_XATTR_HAS_USER_NS "no" CACHE INTERNAL
	"Does posix_acl_from_xattr() have struct user_namespace argument?"
			)
		endif (have_xattr_user_ns_impl)

		set(check_xattr_user_ns_message
"${check_xattr_user_ns_message} - ${POSIX_ACL_XATTR_HAS_USER_NS}"
		)
	endif (DEFINED POSIX_ACL_XATTR_HAS_USER_NS)
	message(STATUS "${check_xattr_user_ns_message}")
endmacro(check_xattr_user_ns)
############################################################################

# Check if hlist_for_each_entry*() macros accept only 'type *pos' argument
# rather than both 'type *tpos' and 'hlist_node *pos' as the loop cursors.
# The macro sets variable 'HLIST_FOR_EACH_ENTRY_POS_ONLY'.
macro(check_hlist_for_each_entry)
	set(check_hlist_for_each_entry_message
"Checking the signatures of hlist_for_each_entry*() macros"
	)
	message(STATUS "${check_hlist_for_each_entry_message}")
	if (DEFINED HLIST_FOR_EACH_ENTRY_POS_ONLY)
		set(check_hlist_for_each_entry_message
"${check_hlist_for_each_entry_message} [cached] - done"
		)
	else ()
		kmodule_try_compile(pos_only_impl
			"${CMAKE_BINARY_DIR}/check_hlist_for_each_entry"
			"${kmodule_test_sources_dir}/check_hlist_for_each_entry/module.c"
		)
		if (pos_only_impl)
			set(HLIST_FOR_EACH_ENTRY_POS_ONLY "yes" CACHE INTERNAL
	"Do hlist_for_each_entry*() macros have only 'type *pos' to use as a loop cursor?"
			)
		else ()
			set(HLIST_FOR_EACH_ENTRY_POS_ONLY "no" CACHE INTERNAL
	"Do hlist_for_each_entry*() macros have only 'type *pos' to use as a loop cursor?"
			)
		endif (pos_only_impl)

		set(check_hlist_for_each_entry_message
			"${check_hlist_for_each_entry_message} - done"
		)
	endif (DEFINED HLIST_FOR_EACH_ENTRY_POS_ONLY)
	message(STATUS "${check_hlist_for_each_entry_message}")
endmacro(check_hlist_for_each_entry)
############################################################################

# Check if 'random32' is available in the kernel.
# The macro sets variable 'KEDR_HAVE_RANDOM32'.
macro(check_random32)
	set(check_random32_message
		"Checking if random32() is available"
	)
	message(STATUS "${check_random32_message}")
	if (DEFINED KEDR_HAVE_RANDOM32)
		set(check_random32_message
"${check_random32_message} [cached] - ${KEDR_HAVE_RANDOM32}"
		)
	else (DEFINED KEDR_HAVE_RANDOM32)
		kmodule_try_compile(have_random32_impl
			"${CMAKE_BINARY_DIR}/check_random32"
			"${kmodule_test_sources_dir}/check_random32/module.c"
		)
		if (have_random32_impl)
			set(KEDR_HAVE_RANDOM32 "yes" CACHE INTERNAL
				"Is random32() available?"
			)
		else (have_random32_impl)
			set(KEDR_HAVE_RANDOM32 "no" CACHE INTERNAL
				"Is random32() available?"
			)
		endif (have_random32_impl)

		set(check_random32_message
"${check_random32_message} - ${KEDR_HAVE_RANDOM32}"
		)
	endif (DEFINED KEDR_HAVE_RANDOM32)
	message(STATUS "${check_random32_message}")
endmacro(check_random32)
############################################################################

# The payload modules should call kedr_find_function() for each kernel 
# function they would like to process. If the function is not known
# to KEDR (absent from the "function database", see "functions/" directory), 
# kedr_find_function() issues a fatal error.
# 
# 'func_name' is the name of the function,
# 'found_var' is the name of the output variable (the variable will evaluate
# as "true" if found, as "false" otherwise). 
function (kedr_find_function func_name found_var)
	if (NOT DEFINED KEDR_DATA_FILE_${func_name})
		message(FATAL_ERROR "Unsupported function: ${func_name}")
	endif (NOT DEFINED KEDR_DATA_FILE_${func_name})
	
	# OK, known function. 
	kmodule_is_function_exist(${func_name} ${found_var})
	if (${found_var})
		# Mark the function as used by at least one payload module.
		# This can be used when preparing the tests for call interception.
		# The actual value does not matter, it only matters that this 
		# variable is defined.
		set(KEDR_FUNC_USED_${func_name} "yes" CACHE INTERNAL
			"Is kernel function ${func_name} used by any payload module?")
		set(${found_var} "${${found_var}}" PARENT_SCOPE)
	else ()
		set(${found_var} "NO" PARENT_SCOPE)
	endif (${found_var})
endfunction (kedr_find_function func_name found_var)

# Returns (in ${path_var}) the path to the .data file for the function 
# ${func_name}. The function must be present in the system and 
# kedr_find_function() must be called for it before kedr_get_data_for_func().
function (kedr_get_data_for_func func_name path_var)
	if (NOT DEFINED KEDR_FUNC_USED_${func_name})
		message(FATAL_ERROR 
"Attempt to lookup data for a function that is not marked as used: "
		"${func_name}")
	endif (NOT DEFINED KEDR_FUNC_USED_${func_name})
	
	set(${path_var} "${KEDR_DATA_FILE_${func_name}}" PARENT_SCOPE)
endfunction (kedr_get_data_for_func func_name path_var)

# kedr_get_header_data_list(list_var func1 [func2 func3 ...])
# Get the list of paths to the 'header.data' files for the given functions.
# The functions must be present in the system and # kedr_find_function() 
# must be called for each of them before kedr_get_header_data_list() is 
# called.
# The resulting list is returned in ${list_var}. It will contain at least 
# one item. It will not contain duplicates.
function (kedr_get_header_data_list list_var)
	set(hdata_list)
	foreach (func ${ARGN})
		if (NOT DEFINED KEDR_FUNC_USED_${func})
			message(FATAL_ERROR 
"Attempt to lookup header data for a function that is not marked as used: "
			"${func}")
		endif (NOT DEFINED KEDR_FUNC_USED_${func})
		list(APPEND hdata_list "${KEDR_HEADER_DATA_FILE_${func}}")
	endforeach ()
	list(REMOVE_DUPLICATES hdata_list)
	
	list(LENGTH hdata_list hdata_list_len)
	if (NOT hdata_list_len)
		message(FATAL_ERROR 
		"kedr_get_header_data_list(): BUG: the output list is empty.")
	endif (NOT hdata_list_len)
	
	set(${list_var} ${hdata_list} PARENT_SCOPE)
endfunction (kedr_get_header_data_list list_var)

# kedr_create_header_rules(
#	header_impl_file header_data_file func1 [func2 ...])
# Create the rules to generate ${header_impl_file} from
# ${header_data_file} and the specific header files for the given
# functions.
function(kedr_create_header_rules header_impl_file header_data_file 
	functions)
	set(hlist_data)
	kedr_get_header_data_list(hlist_data ${functions} ${ARGN})
	to_abs_path(hdata_files_abs ${header_data_file} ${hlist_data})
	add_custom_command(OUTPUT ${header_impl_file}
		COMMAND cat ${hdata_files_abs} > ${header_impl_file}
		DEPENDS ${hdata_files_abs}
	)
endfunction(kedr_create_header_rules header_impl_file header_data_file 
	functions)

# kedr_create_data_rules(func)
# Create the rules to generate ${func}_impl.data from
# ${func}.data (contains processing instructions specific to the current 
# payload) and the .data file from "func_db" (contains function signature).
function(kedr_create_data_rules func)
	set(func_db_data_file)
	kedr_get_data_for_func(${func} func_db_data_file)
	
	set(func_proc_data_file)
	to_abs_path(func_proc_data_file ${func}.data)
	
	add_custom_command(OUTPUT "${func}_impl.data"
		COMMAND printf "\"[group]\\n\"" > "${func}_impl.data"
		COMMAND grep -E -v '\\[group\\]' 
			"${func_db_data_file}" >> "${func}_impl.data"
		COMMAND printf "\"\\n\"" >> "${func}_impl.data"
		COMMAND grep -E -v '\\[group\\]' 
			"${func_proc_data_file}" >> "${func}_impl.data"
		DEPENDS
			"${func_proc_data_file}"
			"${func_db_data_file}"
	)
endfunction(kedr_create_data_rules func)

# kedr_create_payload_module(module_name payload_data_file template_dir)
# Create the rules to build a payload module with the given name.
# 'payload_data_file' defines how the kernel functions are to be processed
# by this payload module.
# 'template_dir' - path to the direcotry containing the templates to be used
# to prepare the source code of the module from 'payload_data_file'.
function(kedr_create_payload_module module_name payload_data_file 
	template_dir)
	kbuild_use_symbols("${CMAKE_BINARY_DIR}/core/Module.symvers")
	kbuild_add_dependencies("kedr")
	
	# Rules to build the module
	kbuild_add_module(${module_name}
		"payload.c"
		"functions_support.c")
		
	# Rules to obtain the source files of the module
	kedr_generate("payload.c" ${payload_data_file} "${template_dir}")
	kedr_generate("functions_support.c" ${payload_data_file} "${KEDR_GEN_TEMPLATES_DIR}/functions_support.c")
endfunction(kedr_create_payload_module module_name payload_data_file 
	template_dir)

# kedr_create_payload_data(payload_data_file func1 [func2 ...])
# Create .data files and the header data files for the functions and prepare
# the main .data file (${payload_data_file}) for the payload module.
function(kedr_create_payload_data header_data_file payload_data_file 
	functions)
	set(header_impl_file "header_impl.data")
	set(functions_data)

	foreach(func ${functions} ${ARGN})
		list(APPEND functions_data "${func}_impl.data")
		kedr_create_data_rules(${func})
	endforeach(func ${functions} ${ARGN})
	
	kedr_create_header_rules(${header_impl_file} ${header_data_file} 
		${functions} ${ARGN})
	
	to_abs_path(payload_data_file_abs ${payload_data_file})
	to_abs_path(source_files_abs ${header_impl_file} ${functions_data})
	
	set(payload_data_file_abs "${CMAKE_CURRENT_BINARY_DIR}/${payload_data_file}")
	add_custom_command(OUTPUT ${payload_data_file_abs}
		COMMAND cat ${source_files_abs} > ${payload_data_file_abs}
		DEPENDS ${source_files_abs}
	)
endfunction(kedr_create_payload_data header_data_file payload_data_file 
	functions)
############################################################################
