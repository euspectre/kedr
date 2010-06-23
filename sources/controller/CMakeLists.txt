set(kmodule_name "kedr_controller")
set(kmodule "${kmodule_name}.ko")
# Should be configured in some way
set(arch "x86")

set(arch_dir "arch/${arch}")

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${arch_dir}/lib)

kbuild_include_directories(
	"${CMAKE_CURRENT_SOURCE_DIR}/${arch_dir}/include"
	"${CMAKE_CURRENT_BINARY_DIR}/${arch_dir}/lib"
	)

kbuild_add_module(${kmodule_name} 
	"controller.c"
    "${arch_dir}/lib/inat.c"
    "${arch_dir}/lib/insn.c"
    "${arch_dir}/lib/inat-tables.h"
    )

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${arch_dir}/lib/inat-tables.h"
			COMMAND LC_ALL=C awk -f "${CMAKE_CURRENT_SOURCE_DIR}/${arch_dir}/tools/gen-insn-attr-x86.awk"
				"${CMAKE_CURRENT_SOURCE_DIR}/${arch_dir}/lib/x86-opcode-map.txt" >
				"${CMAKE_CURRENT_BINARY_DIR}/${arch_dir}/lib/inat-tables.h"
			DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${arch_dir}/lib/x86-opcode-map.txt"
			)

kedr_install_kmodule(${kmodule_name})
kedr_install_symvers(${kmodule_name})

# The tests
add_subdirectory (tests)
