add_subdirectory(control)
add_subdirectory(capture_trace)

if (NOT CMAKE_CROSSCOMPILING)
# Here "kedr_gen" will be built for standalone usage (rather than 
# to build KEDR itself)
	set (KEDR_GEN_INSTALL_PREFIX ${KEDR_INSTALL_PREFIX_EXEC_AUX})
	add_subdirectory(kedr_gen)
endif (NOT CMAKE_CROSSCOMPILING)
