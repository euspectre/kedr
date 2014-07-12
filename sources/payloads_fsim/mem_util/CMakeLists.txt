# Name of the module to be created
set(kmodule_name "kedr_fsim_mem_util")

if(USER_PART)
	kedr_conf_fsim_add_payload(${kmodule_name})
endif(USER_PART)

# The rest is for kernel part only.
if(NOT KERNEL_PART)
	return()
endif(NOT KERNEL_PART)

# The header part of the data file
configure_file("header.data.in"
	"${CMAKE_CURRENT_BINARY_DIR}/header.data")

kmodule_configure_kernel_functions(functions REQUIRED
	"kstrdup"
	"kstrndup"
	"kmemdup"
	
	OPTIONAL "posix_acl_alloc"
	"posix_acl_clone"
	"posix_acl_from_mode"
	"posix_acl_from_xattr"

	OPTIONAL "match_strdup"
)

# TODO: see the notes and TODO in sources/functions/mem_util/CMakelists.txt
kmodule_configure_kernel_functions(posix_acl_changed
	OPTIONAL "__posix_acl_create"
	"__posix_acl_chmod"
)

if (posix_acl_changed)
	list(APPEND functions ${posix_acl_changed})
else(posix_acl_changed)
	kmodule_configure_kernel_functions(posix_acl_functions
		OPTIONAL "posix_acl_create"
		"posix_acl_chmod"
	)
	if (posix_acl_functions)
		list(APPEND functions ${posix_acl_functions})
	endif (posix_acl_functions)
endif(posix_acl_changed)

create_payload_fsim(${kmodule_name} ${functions})

kedr_install_kmodule(${kmodule_name})
