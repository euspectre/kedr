# Name of the module to create
set(kmodule_name "kedr_fsim_indicator_common")

if(USER_PART)
	kedr_conf_fsim_add_indicator(${kmodule_name})
endif(USER_PART)

# The rest is for kernel part only.
if(NOT KERNEL_PART)
	return()
endif(NOT KERNEL_PART)

create_indicator(${kmodule_name})

kedr_install_kmodule(${kmodule_name})
