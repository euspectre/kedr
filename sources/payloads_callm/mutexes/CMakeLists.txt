#name of module created
set(kmodule_name "kedr_cm_mutexes")

#first part of data-file
configure_file("beginning.data.in"
	"${CMAKE_CURRENT_BINARY_DIR}/beginning.data")

# [NB] If CONFIG_DEBUG_LOCK_ALLOC is defined in the kernel configuration,
# *_nested functions may be used instead of mutex_lock, etc.
kmodule_configure_kernel_functions(functions REQUIRED
	"__mutex_init"
	OPTIONAL "mutex_lock"
	"mutex_lock_interruptible"
	"mutex_trylock"
	REQUIRED "mutex_unlock"
)

kbuild_include_directories ("${CMAKE_CURRENT_BINARY_DIR}")
kbuild_add_module(${kmodule_name}
	"payload.c"
	"trace_payload.h")

rules_generate_payload()

kedr_test_add_triggers_include()
kedr_test_add_trigger_for_function(
	"${CMAKE_CURRENT_BINARY_DIR}/${kmodule_name}.ko"
	${functions}
)

rules_build_datafile(${functions})

kedr_install_kmodule(${kmodule_name})

kedr_add_default_cm_payload(${kmodule_name})