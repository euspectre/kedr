# Only check that example can be built.
example_create_build_script("test.sh" leak_check/annotations)

kedr_test_add_script("examples.leak_check.annotations.01" "test.sh")
