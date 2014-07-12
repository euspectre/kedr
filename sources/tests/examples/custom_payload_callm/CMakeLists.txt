# Only check that example can be built.
if(NOT USER_PART)
    return()
endif(NOT USER_PART)

example_create_build_script(test_example_custom_payload_callm "test.sh")

kedr_test_add_script("examples.custom_payload_callm.01" "test.sh")
