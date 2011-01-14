set(kmodule_name "kedr_trace")
set(kmodule "${kmodule_name}.ko")

kbuild_include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
kbuild_add_module(${kmodule_name} 
	"kedr_trace_module.c" "trace_buffer.c" "trace_buffer.h" "trace_file.c" "trace_file.h"
)

kedr_install_kmodule(${kmodule_name})
kedr_install_symvers(${kmodule_name})

# The tests
# add_subdirectory (tests)
