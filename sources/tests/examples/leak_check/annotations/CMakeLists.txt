# Only check that example can be built.
if(NOT USER_PART)
    return()
endif(NOT USER_PART)

example_create_build_script(test_example_leak_check_annotations "test.sh")

kedr_test_add_script("examples.leak_check.annotations.01" "test.sh")
