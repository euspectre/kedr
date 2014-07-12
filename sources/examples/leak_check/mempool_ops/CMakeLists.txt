set(module_name "leak_check_mempool_ops")
set(example_install_dir "${KEDR_EXAMPLE_PATH}/leak_check/mempool_ops")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/payload.data.in"
	"${CMAKE_CURRENT_BINARY_DIR}/payload.data"
	@ONLY)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Kbuild.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
    @ONLY
)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/makefile.in"
    "${CMAKE_CURRENT_BINARY_DIR}/example_makefile"
    @ONLY
)

example_add(example_leak_check_mempool_ops
    "payload.data"
    "makefile" SOURCE "example_makefile"
    "Kbuild"
    "Readme.txt"
)

example_install(TARGETS example_leak_check_mempool_ops
    EXPORT test_example_leak_check_mempool_ops
    DESTINATION ${example_install_dir}
)