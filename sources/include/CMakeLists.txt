# In multi-kernel builds this file will be configured twice:
# user part needs to install it,
# kernel part use it when compile modules.
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/kedr/defs.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/kedr/defs.h"
)

if(NOT USER_PART)
    return()
endif(NOT USER_PART)

# List of include files (relative to 'kedr' subdirectory) to be installed.
set(KEDR_INCLUDE_FILES_INSTALL
    core/kedr.h
    core/kedr_functions_support.h
    calculator/calculator.h
    control_file/control_file.h
    defs.h
    fault_simulation/fault_simulation.h
    trace/trace.h
    util/stack_trace.h
    leak_check/leak_check.h
)

if (NOT CMAKE_CROSSCOMPILING)
# Install rules for include files
    foreach(include_file ${KEDR_INCLUDE_FILES_INSTALL})
	to_abs_path(include_file_abs "kedr/${include_file}")
	get_filename_component(dest_dir "${KEDR_INSTALL_PREFIX_INCLUDE}/${include_file}" PATH)
	install(FILES "${include_file_abs}" DESTINATION "${dest_dir}")
    endforeach(include_file ${KEDR_INCLUDE_FILES_INSTALL})
endif (NOT CMAKE_CROSSCOMPILING)
