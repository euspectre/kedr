if(KEDR_LEAK_CHECK)
    set(NAME_OF_PAYLOAD_LOADED "kedr_lc_common_mm")
else(KEDR_LEAK_CHECK)
    set(NAME_OF_PAYLOAD_LOADED)
endif(KEDR_LEAK_CHECK)

configure_file( "${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
    "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
    @ONLY
)

kedr_test_add_script("control_service.01" "test.sh")