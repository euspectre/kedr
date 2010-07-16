# List of include files(internal ones are commented)
set(KEDR_INCLUDE_FILES
	base/common.h

    module_weak_ref/module_weak_ref.h

    syscall_connector/syscall_connector.h
#    syscall_connector/syscall_connector_internal.h

    fault_simulation/fsim_base.h
    fault_simulation/fsim_indicator_manager.h
#    fault_simulation/fsim_indicator_manager_internal.h
)

# Install rules for each include file
foreach(include_file ${KEDR_INCLUDE_FILES})
    to_abs_path(include_file_abs "kedr/${include_file}")
    string(REGEX REPLACE "/[^/]+$" "" install_dir "${KEDR_INSTALL_PREFIX_INCLUDE}/${include_file}")
    install(FILES ${include_file_abs}
            DESTINATION ${install_dir})
endforeach(include_file ${KEDR_INCLUDE_FILES})