set(module_name "fsim_im_interaction_test_module")

kbuild_include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
kbuild_use_symbols(${CMAKE_BINARY_DIR}/fault_simulation/indicator_manager/kernel_part/Module.symvers)
kbuild_add_module(${module_name} module.c)

kedr_test_add_target(${module_name})