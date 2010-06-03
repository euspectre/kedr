set(kmodule_name "syscall_connector")

kbuild_add_module(${kmodule_name} "syscall_connector.c")

kedr_install_kmodule(${kmodule_name})
kedr_install_symvers(${kmodule_name})