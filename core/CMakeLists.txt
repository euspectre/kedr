set(CORE_MODULE_NAME ${PROJECT_NAME})

# Only the source files from the source tree should be listed here.
set(CORE_SOURCES
	"core.c")

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"
	"${CMAKE_CURRENT_BINARY_DIR}/config.h"
	@ONLY)

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/Kbuild.in"
	"${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
	@ONLY)

foreach(src ${CORE_SOURCES})
	configure_file(
		"${CMAKE_CURRENT_SOURCE_DIR}/${src}"
		"${CMAKE_CURRENT_BINARY_DIR}/${src}"
		COPYONLY)
endforeach()
############################################################################

# Building and installing the module for the current kernel.
# 
# If you need to build the module for a different kernel or using
# a different build process, just copy the sources of the module
# (see ${CORE_SOURCES} above), Kbuild, and config.h somewhere from 
# ${CMAKE_CURRENT_BINARY_DIR} and process them as you wish.
set(CORE_SOURCES_PREPARED 
	"${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
	"${CMAKE_CURRENT_BINARY_DIR}/config.h")
foreach(src ${CORE_SOURCES})
	list(APPEND CORE_SOURCES_PREPARED
		"${CMAKE_CURRENT_BINARY_DIR}/${src}")
endforeach()

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/Makefile.mk.in"
	"${CMAKE_CURRENT_BINARY_DIR}/Makefile.mk"
	@ONLY)

add_custom_target("core_module" ALL
	DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${CORE_MODULE_NAME}.ko")

add_custom_command(
	OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${CORE_MODULE_NAME}.ko"
	COMMAND ${CMAKE_MAKE_PROGRAM} -f Makefile.mk
	DEPENDS ${CORE_SOURCES_PREPARED})

install(FILES
	"${CMAKE_CURRENT_BINARY_DIR}/${CORE_MODULE_NAME}.ko"
	DESTINATION "${KEDR_MODULE_DIR}")
# Note that you may need to call depmod if you would like modprobe to find
# the module.
############################################################################
