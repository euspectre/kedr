# Add fault simulation payload module, used for tests.
# Payload is generated from "payload.data" file, located in the 
# current source or binary directory.
function(add_test_fsim_payload kmodule_name)
    kedr_create_payload_module(${kmodule_name} "payload.data"
    "${KEDR_GEN_TEMPLATES_DIR}/payload_fsim.c")

    kbuild_link_module(${kmodule_name} kedr_fault_simulation)
    kedr_test_install_module(${kmodule_name})
endfunction(add_test_fsim_payload kmodule_name)

set(trigger_target_module_name "kedr_test_trigger_target")
set(indicators_module_name "payloads_fsim_test_indicators")

# Build commmon modules
add_subdirectory(trigger_target)
add_subdirectory(indicators_module)

itesting_path(this_install_dir)

# Common declaration for test scripts
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test_common.sh.in"
    "${CMAKE_CURRENT_BINARY_DIR}/test_common.sh"
    @ONLY)

kedr_test_install(PROGRAMS "test_common.sh")
set(_test_common_install_path "${this_install_dir}/test_common.sh")

# Add test for fault simulation payload.
#
# This is similar to kedr_test_add_script(), but given script
# is executed inside 'test_common.sh'.
#
# 'test_function_script' is installed and passed to test_common.sh
# as command line parameter.
function(add_test_payload_fsim test_name test_function_script)
    kedr_test_install(FILES ${test_function_script})
    
    to_abs_path(path_abs ${test_function_script})
    itesting_path(install_path ${path_abs})
    
    itesting_path(this_install_dir)
    
    file(RELATIVE_PATH path_relative ${this_install_dir} ${install_path})
    file(RELATIVE_PATH test_common_path_relative ${this_install_dir}
        ${_test_common_install_path}
    )
    
    kedr_test_add_script_shared(${test_name} ${test_common_path_relative} ${path_relative})
endfunction(add_test_payload_fsim)

# Add tests themselves
add_subdirectory(test_no_fpoint_params)
add_subdirectory(test_fpoint_with_params)
add_subdirectory(test_several_functions)
add_subdirectory(test_reuse_fpoint)
add_subdirectory(test_rename_fpoint)
add_subdirectory(test_reuse_renamed_fpoint)
add_subdirectory(test_fsim_ellipsis)
add_subdirectory(test_fsim_va_arg)
