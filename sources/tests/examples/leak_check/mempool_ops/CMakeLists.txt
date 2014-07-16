# Only check that example can be built.
example_create_build_script("test.sh" leak_check/mempool_ops)

kedr_test_add_script("examples.leak_check.mempool_ops.01" "test.sh")

