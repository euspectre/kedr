# Only check that example can be built.
example_create_build_script("test.sh" custom_payload_callm)

kedr_test_add_script("examples.custom_payload_callm.01" "test.sh")
