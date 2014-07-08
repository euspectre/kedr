set(module_name "kedr_sample_target")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/makefile_installed.in"
	"${CMAKE_CURRENT_BINARY_DIR}/example_makefile"
	@ONLY)

example_add(example_sample_target
    "cfake.h"
    "cfake.c"
    "Kbuild"
    "kedr_sample_target"
    "makefile" SOURCE "example_makefile"
)

example_install(TARGETS example_sample_target EXPORT test_example_sample_target
    DESTINATION "${KEDR_EXAMPLE_PATH}/sample_target"
    REGEX "kedr_sample_target"
	PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ
		    GROUP_EXECUTE GROUP_READ
		    WORLD_EXECUTE WORLD_READ
)
