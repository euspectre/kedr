# Name of the module to create
set(kmodule_name "kedr_cm_mutexes")

# The header part of the data file
configure_file("header.data.in"
	"${CMAKE_CURRENT_BINARY_DIR}/header.data")

# [NB] If CONFIG_DEBUG_LOCK_ALLOC is defined in the kernel configuration,
# *_nested functions may be used instead of mutex_lock, etc.
kmodule_configure_kernel_functions(functions REQUIRED
	"__mutex_init"
	OPTIONAL "mutex_lock"
	"mutex_lock_interruptible"
	"mutex_lock_killable"
	REQUIRED "mutex_trylock"
	"mutex_unlock"
)

create_payload_callm(${kmodule_name} ${functions})

kedr_install_kmodule(${kmodule_name})

kedr_conf_callm_add_payload(${kmodule_name})
