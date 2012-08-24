func_db_configure_functions (
	"__mutex_init"
	"mutex_lock"
	"mutex_lock_interruptible"
	"mutex_lock_killable"
	"mutex_trylock"
	"mutex_unlock"
)

# [NB] If CONFIG_DEBUG_LOCK_ALLOC is defined in the kernel configuration,
# *_nested functions may be used instead of mutex_lock, etc. 
