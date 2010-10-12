# Configure control file implementation of in-KEDR usage
set(CONTROL_FILE_HEADER "kedr/control_file/control_file.h")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/control_file.c.in"
	"${CMAKE_CURRENT_BINARY_DIR}/control_file.c"
	@ONLY)

add_subdirectory(tests)