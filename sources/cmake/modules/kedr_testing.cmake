########################################################################
# Test-related macros
########################################################################
include(install_testing)
include(install_testing_ctest)

#  kedr_test_init()
#
# Enables testing support and performs other initialization tasks.
#
# Binary directory, where this function is called, below is reffered as
# "binary testing tree".
macro (kedr_test_init)
    set(test_directory "${KEDR_INSTALL_PREFIX_VAR}/tests")
    itesting_init(${test_directory})
    if(USER_PART)
	ictest_enable_testing(${test_directory})
    endif(USER_PART)
endmacro (kedr_test_init)

#  kedr_test_add(<test_name> <app_file> [args ...])
#
# Add test with name <test_name>, which executes <app_file> with given
# arguments.
#
# Directory, obtained with itesting_path(), is used as current directory
# when test is executed.
function (kedr_test_add test_name app_file)
    if(KERNEL_PART_ONLY)
	message(FATAL_ERROR "[Developer error] Tests cannot be added in KERNEL_PART_ONLY builds")
    endif(KERNEL_PART_ONLY)
    ictest_add_test(${test_name} ${app_file} ${ARGN})
endfunction (kedr_test_add)

#  kedr_test_install(FILES|PROGRAMS files ... [PERMISSIONS permissions])
#
# Install files for testing purposes.
#
# Every <file> may refer to file inside:
#  1) binary testing tree (where kedr_test_init() was called)
#  2) current source dir
#
# Relative path is transformed into absolute using to_abs_path()
# mechanism.
#
# Install location for each file is determine using itesting_path()
# mechanism.
#
# Permissions are determined in the same way as for command
#  install(FILES|PROGRAMS)
function(kedr_test_install mode)
    if(NOT mode STREQUAL "FILES" AND NOT mode STREQUAL "PROGRAMS")
	message(FATAL_ERROR "Unknown mode \"${mode}\". Should be either FILES or PROGRAMS.")
    endif()
    cmake_parse_arguments(kedr_test_install "" "" "PERMISSIONS" ${ARGN})
    if(kedr_test_install_PERMISSIONS)
	set(permissions_param PERMISSIONS ${kedr_test_install_PERMISSIONS})
    else(kedr_test_install_PERMISSIONS)
	set(permissions_param)
    endif(kedr_test_install_PERMISSIONS)
    foreach(f ${kedr_test_install_UNPARSED_ARGUMENTS})
	to_abs_path(f_abs ${f})
	itesting_path(f_install_path ${f_abs})
	get_filename_component(f_install_dir ${f_install_path} PATH)
	install(${mode} ${f_abs}
	    DESTINATION ${f_install_dir}
	    ${permissions_param}
	)
    endforeach(f)
endfunction(kedr_test_install)


#  kedr_test_add_script_shared(<test_name> <script_file> [args ...])
#
# Add test with name <test_name>, which executes script
# (a Bash script, actually) <script_file> with given
# arguments.
#
# Script file is expected to be already installed.
function (kedr_test_add_script_shared test_name script_file)
    kedr_test_add(${test_name} /bin/bash ${script_file} ${ARGN})
endfunction (kedr_test_add_script_shared)


#  kedr_test_add_script(<test_name> <script_file> [args ...])
#
# Add test with name <test_name>, which executes script
# (a Bash script, actually) <script_file> with given
# arguments.
#
# <script_file> may refer to file inside:
#  1) binary testing tree (where kedr_test_init() was called)
#  2) current source dir
#
# Relative path is transformed into absolute using to_abs_path()
# mechanism.
#
# In any case, script file is automatically installed as with
#  kedr_test_install(PROGRAMS ${script_file})
# and test actually executes installed script file.
function (kedr_test_add_script test_name script_file)
    to_abs_path(script_file_abs ${script_file})
    itesting_path(script_install_file ${script_file_abs})
    get_filename_component(script_install_dir ${script_install_file} PATH)
    install(PROGRAMS ${script_file_abs} DESTINATION ${script_install_dir})
    
    # For execute script, use its relative path
    itesting_path(this_install_dir)
    file(RELATIVE_PATH script_file_relative ${this_install_dir} ${script_install_file})
    
    kedr_test_add_script_shared(${test_name} ${script_file_relative} ${ARGN})
endfunction (kedr_test_add_script)


#  kedr_test_install_module(module_name ...)
#
# Install kernel module(s) for testing purposes.
#
# Install location is selected according to build location of the module
# plus %kernel%/ sudirectory, which is evaluated to kernel version.
function (kedr_test_install_module module_name)
    get_property(is_module TARGET ${module_name} PROPERTY KMODULE_TYPE SET)
    if(NOT is_module)
	message(FATAL_ERROR "${module_name} is not a kernel module target")
    endif(NOT is_module)
    get_property(module_location TARGET ${module_name} PROPERTY KMODULE_MODULE_LOCATION)
    if(NOT module_location)
	message(FATAL_ERROR "kedr_test_install_module: Do not process imported kernel module target ${module_name}")
    endif(NOT module_location)
    get_filename_component(module_build_dir ${module_location} PATH)
    itesting_path(module_install_dir ${module_build_dir})
    kbuild_install(TARGETS ${module_name}
	MODULE DESTINATION ${module_install_dir}/${Kbuild_VERSION_STRING}
    )
endfunction (kedr_test_install_module)
