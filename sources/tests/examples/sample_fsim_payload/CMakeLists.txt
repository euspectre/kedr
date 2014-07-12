# Actually, this example is also built in sample_fsim/ test.
# But keep this test for better tests traceability.

# Only check that example can be built.
if(NOT USER_PART)
    return()
endif(NOT USER_PART)

example_create_build_script(test_example_sample_fsim_payload "test.sh")

kedr_test_add_script("examples.sample_fsim_payload.01" "test.sh")
