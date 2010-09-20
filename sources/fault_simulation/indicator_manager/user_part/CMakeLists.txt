set(library_name kedr_fsim_indicator_manager_u)
set(VERSION_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/versions.ldscript)

add_library(${library_name} SHARED fsim_indicator_manager.c)
target_link_libraries(${library_name} kedr_syscall_connector_u)

set_target_properties(${library_name} PROPERTIES
	SOVERSION "1"
	VERSION "1.0.0"
	LINK_FLAGS "-Wl -z -defs -Wl --version-script=${VERSION_SCRIPT}"
	)