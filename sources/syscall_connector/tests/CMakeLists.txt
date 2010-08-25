

configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/test_module_simple.sh.in"
  "${CMAKE_CURRENT_BINARY_DIR}/test_module_simple.sh"
  @ONLY
)

kedr_test_add_script (syscall_connector.01 
   test_module_simple.sh
)

subdirs(load_library)
subdirs(use_library)
subdirs(use_named_libraries_service)
