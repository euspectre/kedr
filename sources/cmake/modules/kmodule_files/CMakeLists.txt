# Needed for all CMakeLists files, otherwise cmake will warn
cmake_minimum_required (VERSION 2.6)

if (NOT KERNELDIR)
	set(KERNELDIR /lib/modules/${KBUILD_VERSION_STRING}/build)
endif (NOT KERNELDIR)

set(PWD $\(shell pwd\))

project(hello)
# Create rule for produce file from another file via copiing
macro(copy_source_file FILENAME SOURCE)
add_custom_command(OUTPUT "${FILENAME}"
					COMMAND cp -p ${SOURCE} ${FILENAME}
					MAIN_DEPENDENCY "${SOURCE}")
endmacro(copy_source_file FILENAME SOURCE)

if(NOT DEFINED SRC_FILE)
	message(FATAL_ERROR "'SRC_FILE' should be defined, but it is not.")
endif(NOT DEFINED SRC_FILE)

configure_file(Kbuild.in Kbuild)
# 'make all' should produce try_compile.ko file
add_custom_target(kmodule_compile ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/try_compile.ko)
# Rule to produce try_compile.ko from try_compile.c
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/try_compile.ko"
	COMMAND $(MAKE) ARCH=${KEDR_ARCH} CROSS_COMPILE=${KEDR_CROSS_COMPILE} -C ${KERNELDIR} M=${PWD} modules
					DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/try_compile.c")

# Kbuild system cannot work with out-of-source build,
# so copy source file into build dir and rename it to try_compile.c.
copy_source_file("${CMAKE_CURRENT_BINARY_DIR}/try_compile.c" "${SRC_FILE}")
