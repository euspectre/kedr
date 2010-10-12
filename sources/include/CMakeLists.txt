# List of include files(internal ones are commented)
set(KEDR_INCLUDE_FILES
	base/common.h

    fault_simulation/fault_simulation.h
	fault_simulation/calculator.h
	control_file/control_file.h
)

# Install rules for each include file
foreach(include_file ${KEDR_INCLUDE_FILES})
    to_abs_path(include_file_abs "kedr/${include_file}")
    string(REGEX REPLACE "/[^/]+$" "" install_dir "${KEDR_INSTALL_PREFIX_INCLUDE}/${include_file}")
    install(FILES ${include_file_abs}
            DESTINATION ${install_dir})
endforeach(include_file ${KEDR_INCLUDE_FILES})
