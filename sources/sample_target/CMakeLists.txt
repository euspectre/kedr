set(module_name "kedr_sample_target")

kbuild_include_directories(${CMAKE_CURRENT_SOURCE_DIR})
kbuild_add_module(${module_name} "cfake.c")

kedr_install_kmodule(${module_name})

set(KEDR_SAMPLE_TARGET "${KEDR_INSTALL_PREFIX_KMODULE}/kedr_sample_target.ko")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/kedr_sample_target.in"
            "${CMAKE_CURRENT_BINARY_DIR}/kedr_sample_target"
            @ONLY)

#For testing

set(KEDR_SAMPLE_TARGET "${CMAKE_BINARY_DIR}/sample_target/kedr_sample_target.ko")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/kedr_sample_target.in"
            "${CMAKE_CURRENT_BINARY_DIR}/kedr_test_sample_target"
            @ONLY)


install(FILES "${CMAKE_CURRENT_BINARY_DIR}/kedr_sample_target"
        DESTINATION "${KEDR_INSTALL_PREFIX_EXEC}"
        PERMISSIONS  OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ OWNER_EXECUTE GROUP_EXECUTE WORLD_EXECUTE)