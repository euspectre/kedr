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
	"mutex_lock_killable"
	"mutex_trylock"
	REQUIRED "mutex_unlock"
)

create_payload_callm(${kmodule_name} ${functions})

kedr_install_kmodule(${kmodule_name})

kedr_conf_callm_add_payload(${kmodule_name})

kedr_test_add_subdirectory(tests)