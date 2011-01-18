# Needed for all CMakeList files, otherwise cmake will warn
cmake_minimum_required (VERSION 2.6)

set(KERNELDIR /lib/modules/$\(shell uname -r\)/build)
set(PWD $\(shell pwd\))

project(hello)
# Create rule for produce file from another file via copiing
macro(copy_source_file FILENAME SOURCE)
add_custom_command(OUTPUT "${FILENAME}"
					COMMAND cp -p ${SOURCE} ${FILENAME}
					MAIN_DEPENDENCY "${SOURCE}")
endmacro(copy_source_file FILENAME SOURCE)

if(NOT DEFINED SRC_FILE)
	message(FATAL_ERROR "'SRC_FILE' should be defined, but it doesn't.")
endif(NOT DEFINED SRC_FILE)

configure_file(Kbuild.in Kbuild)
# 'make all' should produce hello.ko file
add_custom_target(kmodule_compile ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/try_compile.ko)
# Rule to produce hello.ko from hello.c
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/try_compile.ko"
					COMMAND $(MAKE) -C ${KERNELDIR} M=${PWD} modules
					DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/try_compile.c")
# Kbuild system cannot work with out-of-source build,
# so copy source file into build dir.
copy_source_file("${CMAKE_CURRENT_BINARY_DIR}/try_compile.c" "${SRC_FILE}")
