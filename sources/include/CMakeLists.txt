configure_file("${CMAKE_CURRENT_SOURCE_DIR}/kedr/defs.h.in"
	"${CMAKE_CURRENT_BINARY_DIR}/kedr/defs.h")

# List of include files, which needn't to be configured and
# should be copied into binary tree as is.
set(KEDR_INCLUDE_FILES_SRC
	base/common.h
	calculator/calculator.h
	control_file/control_file.h
    fault_simulation/fault_simulation.h
	trace/trace.h
	util/stack_trace.h
	wobject/wobject.h
)


# List of include files, which should be installed.
set(KEDR_INCLUDE_FILES_INSTALL
	defs.h
	base/common.h
    fault_simulation/fault_simulation.h
	calculator/calculator.h
	control_file/control_file.h
	util/stack_trace.h
	trace/trace.h
)

# Copy headers from src-tree into binary tree
foreach(include_file ${KEDR_INCLUDE_FILES_SRC})
    get_filename_component(dest_dir "${CMAKE_CURRENT_BINARY_DIR}/kedr/${include_file}" PATH)
	file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/kedr/${include_file}"
		DESTINATION "${dest_dir}")
endforeach(include_file ${KEDR_INCLUDE_FILES_SRC})


# Install rules for include files(files will be installed from binary tree!)
foreach(include_file ${KEDR_INCLUDE_FILES_INSTALL})
	get_filename_component(dest_dir "${KEDR_INSTALL_PREFIX_INCLUDE}/${include_file}" PATH)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/kedr/${include_file}"
            DESTINATION "${dest_dir}")

endforeach(include_file ${KEDR_INCLUDE_FILES_INSTALL})

# Copy headers files for examples.
#
# Because examples may used only installed header files, these files
# are copiied into directory different from one for files used by
# components of the KEDR internally. These strategy allows to detect 
# missing install headers at the testing stage.
foreach(include_file ${KEDR_INCLUDE_FILES_INSTALL})
    get_filename_component(dest_dir "${CMAKE_CURRENT_BINARY_DIR}/install/kedr/${include_file}" PATH)
	file(COPY "${CMAKE_CURRENT_BINARY_DIR}/kedr/${include_file}"
		DESTINATION "${dest_dir}")
endforeach(include_file ${KEDR_INCLUDE_FILES_SRC})
