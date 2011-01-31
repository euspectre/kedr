#name of module created
set(kmodule_name "kedr_cm_spinlocks")

#first part of data-file
configure_file("beginning.data.in"
	"${CMAKE_CURRENT_BINARY_DIR}/beginning.data")

option(enable_full_spinlock "Intercept all spinlock-related function" OFF)
#SET(enable_full_spinlock CACHE BOOL "Intercept all spinlock-related function")

kmodule_configure_kernel_functions(functions REQUIRED
	ONE_OF_BEGIN "_spin_lock_irqsave" "_raw_spin_lock_irqsave" ONE_OF_END
	ONE_OF_BEGIN "_spin_unlock_irqrestore" "_raw_spin_unlock_irqrestore" ONE_OF_END
)
if(enable_full_spinlock)
	kmodule_configure_kernel_functions(functions_optional REQUIRED
		ONE_OF_BEGIN "_spin_lock" "_raw_spin_lock" ONE_OF_END
		ONE_OF_BEGIN "_spin_lock_irq" "_raw_spin_lock_irq" ONE_OF_END
		ONE_OF_BEGIN "_spin_unlock" "_raw_spin_unlock" ONE_OF_END
		ONE_OF_BEGIN "_spin_unlock_irq" "_raw_spin_unlock_irq" ONE_OF_END
	)
	list(APPEND functions ${functions_optional})
endif(enable_full_spinlock)


kbuild_include_directories ("${CMAKE_CURRENT_BINARY_DIR}")
kbuild_add_module(${kmodule_name}
	"payload.c")

rules_generate_payload()

rules_build_datafile(${functions})

kedr_test_add_ci_include()
kedr_test_add_ci_test(${functions})

kedr_install_kmodule(${kmodule_name})

kedr_conf_callm_add_payload(${kmodule_name})
kedr_conf_fsim_add_payload(${kmodule_name})