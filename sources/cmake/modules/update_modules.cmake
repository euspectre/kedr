# Update kernel modules dependencies at 'install' stage.
# This allow to use 'modprobe' with installed modules.
#
# Should be included after all kernel modules are issued to be installed.
#
# If UPDATE_MODULES_UNINSTALL_AFTER variable is given, also
# update dependencies at 'uninstall' stage.
#
# There is no user interface, defined by this cmake module.
# Whole functionality is added immediately when module is included.
#
#
# Variables, which affect behaviour:
#
#  UPDATE_MODULES_UNINSTALL_AFTER - Target built at 'uninstall'
#    stage, after which modules dependencies should be updated.
#    Normally, this is target which uninstall kernel modules.
#
#    If 'uninstall' target is created with cmake module
#     cmake_uninstall.cmake
#    value of variable should be "uninstall_files".
#
#    If variable is not set, kernel modules dependencies are not updated
#    at 'uninstall' stage.
get_filename_component(update_module_this_dir ${CMAKE_CURRENT_LIST_FILE} PATH)

# Script passed to install(SCRIPT) in current directory will be 
# executed BEFORE any file in subdirectory will be installled.
#
# For make script executed AFTER files be installed,
# we need to call install(SCRIPT) from subdirectory.
add_subdirectory(
    "${update_module_this_dir}/update_modules_files/update_modules_dir"
    update_modules
)

if(UPDATE_MODULES_UNINSTALL_AFTER)
    # Also execute depmod after uninstall.
    add_custom_target(uninstall_depmod
        COMMAND ${CMAKE_COMMAND} -P
        "${update_module_this_dir}/update_modules_files/update_modules_dir/depmod.cmake"
    )
    # New target should be executed for complete 'uninstall'..
    add_dependencies(uninstall uninstall_depmod)
    # ... but it should be executed before target, which actually remove modules.
    add_dependencies(uninstall_depmod ${UPDATE_MODULES_UNINSTALL_AFTER})
endif(UPDATE_MODULES_UNINSTALL_AFTER)