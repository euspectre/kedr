set(kmodule_name "kedr_base")
set(kmodule "${kmodule_name}.ko")

kbuild_add_module(${kmodule_name} 
	"base.c"
)

kedr_install_kmodule(${kmodule_name})
kedr_install_symvers(${kmodule_name})
