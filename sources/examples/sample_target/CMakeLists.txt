set(module_name "kedr_sample_target")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/makefile_installed.in"
	"${CMAKE_CURRENT_BINARY_DIR}/makefile_installed"
	@ONLY)

install(FILES   "${CMAKE_CURRENT_SOURCE_DIR}/cfake.h"
                "${CMAKE_CURRENT_SOURCE_DIR}/cfake.c"
                "${CMAKE_CURRENT_SOURCE_DIR}/Kbuild"
        DESTINATION "${KEDR_EXAMPLE_PATH}/sample_target"
)
install(FILES   "${CMAKE_CURRENT_SOURCE_DIR}/kedr_sample_target"
        DESTINATION "${KEDR_EXAMPLE_PATH}/sample_target"
        PERMISSIONS  OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ OWNER_EXECUTE GROUP_EXECUTE WORLD_EXECUTE
)
install(FILES   "${CMAKE_CURRENT_BINARY_DIR}/makefile_installed"
        DESTINATION "${KEDR_EXAMPLE_PATH}/sample_target"
        RENAME makefile
)

kedr_test_add_subdirectory(tests)
