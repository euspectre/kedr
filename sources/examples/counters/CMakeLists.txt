set(module_name "kedr_counters")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Kbuild.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
    @ONLY
)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/makefile.in"
    "${CMAKE_CURRENT_BINARY_DIR}/example_makefile"
    @ONLY
)

example_add(example_counters
    "counters.c"
    "counters.h"
    "configure_kernel_functions.sh"
    "functions_common.data"
    "mutex_lock.data"
    "mutex_lock_interruptible.data"
    "mutex_lock_killable.data"
    "makefile" SOURCE "example_makefile"
    "Kbuild"
    "README"
)

example_install(TARGETS example_counters EXPORT test_example_counters
    DESTINATION "${KEDR_EXAMPLE_PATH}/counters"
    REGEX "configure_kernel_functions.sh"
        PERMISSIONS OWNER_WRITE OWNER_READ OWNER_EXECUTE
                    GROUP_READ GROUP_EXECUTE
                    WORLD_READ WORLD_EXECUTE
)
