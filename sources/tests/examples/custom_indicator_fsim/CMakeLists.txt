# Only check that example can be built.
example_create_build_script(test_example_custom_indicator_fsim "test.sh")

kedr_test_add_script("examples.custom_indicator_fsim.01" "test.sh")
