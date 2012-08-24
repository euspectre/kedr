# Name of the module to create
set(kmodule_name "kedr_fsim_indicator_kmalloc")

create_indicator(${kmodule_name})

kedr_install_kmodule(${kmodule_name})

