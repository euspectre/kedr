# The sources will be copied to the build tree.
set(files_to_copy
	"cfake.c"
	"cfake.h"
	"Makefile.mk"
)
# [NB] To avoid confusion with the make file cmake generates, I renamed the
# makefile for the module Makefile.mk. Use 'make -f Makefile.mk' in the
# build tree to build the module.

set(PLUGIN_PATH "${CMAKE_BINARY_DIR}/i13n/plugin/${PLUGIN_NAME}.so")

foreach(to_copy ${files_to_copy})
	configure_file(
		"${CMAKE_CURRENT_SOURCE_DIR}/${to_copy}"
		"${CMAKE_CURRENT_BINARY_DIR}/${to_copy}"
		COPYONLY
	)
endforeach()

configure_file(
	"${CMAKE_SOURCE_DIR}/i13n/kedr_stubs.c"
	"${CMAKE_CURRENT_BINARY_DIR}/kedr_stubs.c"
	COPYONLY
)

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/Kbuild.in"
	"${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
	@ONLY
)
