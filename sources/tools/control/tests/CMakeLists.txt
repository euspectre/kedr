if(KEDR_STANDARD_CALLM_PAYLOADS)
set(NAME_OF_PAYLOAD_LOADED "kedr_cm_cmm")
else(KEDR_STANDARD_CALLM_PAYLOADS)
set(NAME_OF_PAYLOAD_LOADED)
endif(KEDR_STANDARD_CALLM_PAYLOADS)

configure_file( "${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
                "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
                @ONLY)

kedr_test_add_script("control_service.01" "test.sh")