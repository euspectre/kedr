# Name of the module to create
set(kmodule_name "kedr_cm_spinlocks")

# The header part of the data file
configure_file("header.data.in"
	"${CMAKE_CURRENT_BINARY_DIR}/header.data")

option(enable_full_spinlock 
	"Intercept more complete set of spinlock-related function" 
	OFF)

kmodule_configure_kernel_functions(functions REQUIRED
	ONE_OF_BEGIN "_spin_lock_irqsave" "_raw_spin_lock_irqsave" ONE_OF_END
	ONE_OF_BEGIN "_spin_unlock_irqrestore" "_raw_spin_unlock_irqrestore" ONE_OF_END
)
if(enable_full_spinlock)
	kmodule_configure_kernel_functions(functions_optional OPTIONAL
		ONE_OF_BEGIN "_spin_lock" "_raw_spin_lock" ONE_OF_END
		ONE_OF_BEGIN "_spin_lock_irq" "_raw_spin_lock_irq" ONE_OF_END
		ONE_OF_BEGIN "_spin_unlock" "_raw_spin_unlock" ONE_OF_END
		ONE_OF_BEGIN "_spin_unlock_irq" "_raw_spin_unlock_irq" ONE_OF_END
	)
	list(APPEND functions ${functions_optional})
endif(enable_full_spinlock)


create_payload_callm(${kmodule_name} ${functions})

kedr_install_kmodule(${kmodule_name})

kedr_conf_callm_add_payload(${kmodule_name})
