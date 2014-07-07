set(instrumentor_module_name "instrumentor_module")
set(test_module_name "instrumentor_test_module")

kbuild_include_directories(${CORE_SOURCE_DIR}/${arch_dir}/include)
kbuild_include_directories(${CORE_DIR}/${arch_dir}/lib)

rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/kedr_instrumentor.c"
    "${CORE_SOURCE_DIR}/kedr_instrumentor.c")

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${arch_dir}/lib")
rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/${arch_dir}/lib/inat.c"
    "${CORE_SOURCE_DIR}/${arch_dir}/lib/inat.c")

rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/${arch_dir}/lib/insn.c"
    "${CORE_SOURCE_DIR}/${arch_dir}/lib/insn.c")

kbuild_include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
kbuild_add_module(${instrumentor_module_name}
    "instrumentor_module_simple.c"
    "kedr_instrumentor.c"
    
    "${CMAKE_CURRENT_BINARY_DIR}/${arch_dir}/lib/inat.c"
    "${CMAKE_CURRENT_BINARY_DIR}/${arch_dir}/lib/insn.c"
    "${CORE_DIR}/${arch_dir}/lib/inat-tables.h"
)

# We need ${arch_dir}/lib/inat-tables.h, which is generated when
# original core module is built.
add_dependencies(${instrumentor_module_name} kedr)

kedr_test_install_module(${instrumentor_module_name})

add_subdirectory(test_module)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
    "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
    @ONLY
)

kedr_test_add_script("core.components.instrumentor.01"
    "test.sh"
)

