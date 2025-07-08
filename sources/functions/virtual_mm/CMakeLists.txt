configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/__vmalloc.data.${__VM_VER}"
	"${CMAKE_CURRENT_BINARY_DIR}/__vmalloc.data"
	COPYONLY
)

func_db_configure_functions (
	"vmalloc"
	"vmalloc_user"
	"vmalloc_node"
	"vmalloc_32"
	"vmalloc_32_user"
	"vfree"
	"vzalloc"
	"vzalloc_node"

	GENERATED
	"__vmalloc"
)
