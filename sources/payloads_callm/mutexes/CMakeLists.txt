#name of module created
set(kmodule_name "kedr_cm_mutexes")
#really harcoded, because of mist engine templates
#set(kmodule_source_file "payload.c")
#set(kmodule_header_file "trace_payload.h")
#name of data file
set(kmodule_data_file "payload.data")

#first part of data-file
configure_file("beginning.data.in"
	"${CMAKE_CURRENT_BINARY_DIR}/beginning.data")

#__mutex_init
kmodule_configure_kernel_functions(__mutex_init_data
	"__mutex_init" 		"__mutex_init.data")
if(NOT __mutex_init_data)
	message(FATAL_ERROR "__mutex_init is absent in the kernel.")
endif(NOT __mutex_init_data)
#mutex_lock
kmodule_configure_kernel_functions(mutex_lock_data
	"mutex_lock" 		"mutex_lock.data")
if(NOT mutex_lock_data)
	message(FATAL_ERROR "mutex_lock is absent in the kernel.")
endif(NOT mutex_lock_data)
#mutex_lock_interruptible
kmodule_configure_kernel_functions(mutex_lock_interruptible_data
	"mutex_lock_interruptible" 		"mutex_lock_interruptible.data")
if(NOT mutex_lock_interruptible_data)
	message(FATAL_ERROR "mutex_lock_interruptible is absent in the kernel.")
endif(NOT mutex_lock_interruptible_data)
#mutex_trylock
kmodule_configure_kernel_functions(mutex_trylock_data
	"mutex_trylock" 		"mutex_trylock.data")
if(NOT mutex_trylock_data)
	message(FATAL_ERROR "mutex_trylock is absent in the kernel.")
endif(NOT mutex_trylock_data)
#mutex_unlock
kmodule_configure_kernel_functions(mutex_unlock_data
	"mutex_unlock" 		"mutex_unlock.data")
if(NOT mutex_unlock_data)
	message(FATAL_ERROR "mutex_unlock is absent in the kernel.")
endif(NOT mutex_unlock_data)

set(functions_data
	${__mutex_init_data}
	${mutex_lock_data}
	${mutex_lock_interruptible_data}
	${mutex_trylock_data}
	${mutex_unlock_data}
	)


kbuild_include_directories ("${CMAKE_CURRENT_BINARY_DIR}")
kbuild_add_module(${kmodule_name}
	"payload.c"
	"trace_payload.h")

rules_generate_payload(${CMAKE_CURRENT_BINARY_DIR}/${kmodule_data_file}
						${CMAKE_CURRENT_BINARY_DIR})

rules_build_datafile(${kmodule_data_file} "beginning.data"
	${functions_data})

kedr_install_kmodule(${kmodule_name})