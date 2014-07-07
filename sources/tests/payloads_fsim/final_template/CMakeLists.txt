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
set(indicator_constant_module_name "kedr_indicator_constant")
set(indicator_greater_than_module_name "kedr_indicator_greater_than")

# Build commmon modules
add_subdirectory(trigger_target)
add_subdirectory(indicator_constant)
add_subdirectory(indicator_greater_than)

itesting_path(this_install_dir)

# Common declaration for test scripts
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test_common.sh.in"
    "${CMAKE_CURRENT_BINARY_DIR}/test_common.sh"
    @ONLY)

kedr_test_install(PROGRAMS "test_common.sh")

# Add tests themselves
add_subdirectory(test_no_fpoint_params)
add_subdirectory(test_fpoint_with_params)
add_subdirectory(test_several_functions)
add_subdirectory(test_reuse_fpoint)
add_subdirectory(test_rename_fpoint)
add_subdirectory(test_reuse_renamed_fpoint)
add_subdirectory(test_fsim_ellipsis)
add_subdirectory(test_fsim_va_arg)
