add_subdirectory(simple_ordering)
add_subdirectory(cross_cpu_ordering)

if(USER_PART)
    kedr_test_install(PROGRAMS "verify_trace_format.awk")
endif(USER_PART)