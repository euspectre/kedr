configure_file("${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
    "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
    @ONLY
)

kedr_test_add_script("module_weak_ref.simple_load.01"
    "test.sh" load_only)

kedr_test_add_script("module_weak_ref.ref.01"
    "test.sh" ref)

kedr_test_add_script("module_weak_ref.unref.01"
    "test.sh" unref)

kedr_test_add_script("module_weak_ref.wait.01"
    "test.sh" wait)

add_subdirectory(module_a)
add_subdirectory(module_b)
