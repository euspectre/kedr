add_subdirectory(simple_ordering)
add_subdirectory(cross_cpu_ordering)

kedr_test_install(PROGRAMS "verify_trace_format.awk")
