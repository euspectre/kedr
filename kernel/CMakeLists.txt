# To simplify building of the modules with DKMS and such, CMake only
# prepares the header file with the configuration definitions and the
# main makefile(Makefile.mk).
#
# That makefile handles the rest of the build process for the kernel
# modules, CMake is not involved there. The makefiles in the subdirectories
# are used as is.
############################################################################

set(KERNEL_MODULES
	"${CMAKE_CURRENT_BINARY_DIR}/kernel/kedr/kedr.ko"
)

# Local build - build everything for the currently running kernel.
add_custom_target("kedr_kernel_modules" ALL
	DEPENDS ${KERNEL_MODULES})

add_custom_command(
	OUTPUT ${KERNEL_MODULES}
	COMMAND cp -pr "${CMAKE_CURRENT_SOURCE_DIR}/kernel" "${CMAKE_CURRENT_BINARY_DIR}/"
	COMMAND cp -pr "${CMAKE_CURRENT_SOURCE_DIR}/arch" "${CMAKE_CURRENT_BINARY_DIR}/"
	COMMAND cp -pr "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_BINARY_DIR}/"
	COMMAND ${CMAKE_MAKE_PROGRAM} -f Makefile.mk
	WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

set(MODULE_INSTALL_DIR "/lib/modules/${CMAKE_SYSTEM_VERSION}/extra/kedr/")
if (NOT ${CMAKE_INSTALL_PREFIX} STREQUAL "/usr")
	set(MODULE_INSTALL_DIR
		"${CMAKE_INSTALL_PREFIX}${MODULE_INSTALL_DIR}")
endif ()

install(FILES
	${KERNEL_MODULES}
	DESTINATION "${MODULE_INSTALL_DIR}")

# Note that you may need to call depmod if you would like modprobe to find
# the module(s) built here.
############################################################################
