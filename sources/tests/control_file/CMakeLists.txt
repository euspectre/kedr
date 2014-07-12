set(module_name "kedr_control_file_test_module")

if(KERNEL_PART)
	# Currently, only try to build module which use control file architecture
	kbuild_add_module(${module_name} module.c control_file.c)

	rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/control_file.c"
		"${CMAKE_SOURCE_DIR}/control_file/control_file.c")

	kedr_test_install_module(${module_name})
endif(KERNEL_PART)