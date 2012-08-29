# Name of the kernel module
set(kmodule_name "kedr_lc_kasprintf")
########################################################################

# Claim that these functions are used by this payload module
kmodule_configure_kernel_functions(functions 
	OPTIONAL 
	"kasprintf"
	"kvasprintf"
)
########################################################################

kbuild_use_symbols("${CMAKE_BINARY_DIR}/core/Module.symvers")
kbuild_add_dependencies("kedr")
kbuild_use_symbols("${CMAKE_BINARY_DIR}/leak_check/core/Module.symvers")
kbuild_add_dependencies("kedr_leak_check")

# Rules to build the module.
# The payload module is built in a special way because it must handle 
# kasprintf which has a variable argument list.
kbuild_add_module(${kmodule_name}
	"payload.c"
)

kedr_install_kmodule(${kmodule_name})
########################################################################
