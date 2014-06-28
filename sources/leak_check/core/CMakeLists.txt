# Name of the kernel module for the LeakCheck core
set(kmodule_name "kedr_leak_check")

rule_copy_file("stack_trace.c"
	"${CMAKE_SOURCE_DIR}/util/stack_trace/stack_trace.c")

kbuild_include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
kbuild_add_module(${kmodule_name}
# Sources	
	"leak_check.c"
	"klc_output.c"
	"stack_trace.c"

# Headers (list them here to establish appropriate dependencies)
	"leak_check_impl.h"
	"klc_output.h"
)
kbuild_link_module(${kmodule_name} kedr)

kedr_install_kmodule(${kmodule_name})
kedr_install_symvers(${kmodule_name})
########################################################################

