set(kmodule_name "kedr")

# In the future, it should be configurable somehow
set(arch "x86")

set(arch_dir "arch/${arch}")

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${arch_dir}/lib)

set(checking_message "Checking the ways to create module parameters with callbacks")
message(STATUS "${checking_message}")
if((DEFINED MODULE_PARAM_CREATE_USE_OPS_STRUCT) OR (DEFINED MODULE_PARAM_CREATE_USE_OPS))
	set(checking_message "${checking_message} [cached] - done")
else((DEFINED MODULE_PARAM_CREATE_USE_OPS_STRUCT) OR (DEFINED MODULE_PARAM_CREATE_USE_OPS))
	kmodule_try_compile(is_module_param_create_use_ops_struct # RESULT_VAR
		"${CMAKE_CURRENT_BINARY_DIR}/module_param_create_use_ops_struct" # bindir
		"module_param_create_use_ops_struct.c" # srcfile
		OUTPUT_VARIABLE out_ops_struct)

	if(is_module_param_create_use_ops_struct)
		set(checking_message "${checking_message} - done")
		set(MODULE_PARAM_CREATE_USE_OPS_STRUCT "TRUE" CACHE INTERNAL
			"To create module parameters with callbacks, one needs to use struct ops")
	else(is_module_param_create_use_ops_struct)
		kmodule_try_compile(is_module_param_create_use_ops # RESULT_VAR
			"${CMAKE_CURRENT_BINARY_DIR}/module_param_create_use_ops" # bindir
			"module_param_create_use_ops.c" # srcfile
			OUTPUT_VARIABLE out_ops)
		if(is_module_param_create_use_ops)
			set(checking_message "${checking_message} - done")
			set(MODULE_PARAM_CREATE_USE_OPS "TRUE" CACHE INTERNAL
			"To create module parameters with callbacks, one needs to use ops")
		else(is_module_param_create_use_ops)
			set(checking_message "${checking_message} - error")
			message("${checking_message}")
			message("Unknown way to create module param with callbacks.")
			message("Output of test compilation for operations structure:")
			message("${out_ops_srtuct}")
			message("Output of test compilation for operations:")
			message("${out_ops}")
			message(FATAL_ERROR "")
		endif(is_module_param_create_use_ops)
	endif(is_module_param_create_use_ops_struct)
endif((DEFINED MODULE_PARAM_CREATE_USE_OPS_STRUCT) OR (DEFINED MODULE_PARAM_CREATE_USE_OPS))
message(STATUS "${checking_message}")


kbuild_include_directories(
	"${CMAKE_CURRENT_SOURCE_DIR}/${arch_dir}/include"
	"${CMAKE_CURRENT_BINARY_DIR}/${arch_dir}/lib"
	"${CMAKE_CURRENT_SOURCE_DIR}"
	)

kbuild_add_module(${kmodule_name}
	"kedr_module.c"
    "kedr_base.c"
    "kedr_instrumentor.c"
    "kedr_functions_support.c"
    "kedr_target_detector.c"

	"kedr_base_internal.h"
	"kedr_instrumentor_internal.h"
	"kedr_functions_support_internal.h"
	"kedr_target_detector_internal.h"

    "${arch_dir}/lib/inat.c"
    "${arch_dir}/lib/insn.c"
    "${arch_dir}/lib/inat-tables.h"
    )

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${arch_dir}/lib/inat-tables.h"
			COMMAND LC_ALL=C awk -f "${CMAKE_CURRENT_SOURCE_DIR}/${arch_dir}/tools/gen-insn-attr-x86.awk"
				"${CMAKE_CURRENT_SOURCE_DIR}/${arch_dir}/lib/x86-opcode-map.txt" >
				"${CMAKE_CURRENT_BINARY_DIR}/${arch_dir}/lib/inat-tables.h"
			DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${arch_dir}/lib/x86-opcode-map.txt"
			)

kedr_install_kmodule(${kmodule_name})
kedr_install_symvers(${kmodule_name})

# The tests
kedr_test_add_subdirectory (tests)
