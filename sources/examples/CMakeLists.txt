# The path where the examples will be installed
set (KEDR_EXAMPLE_PATH "${KEDR_INSTALL_PREFIX_EXAMPLES}")

add_subdirectory(sample_target)
add_subdirectory(sample_indicator)
add_subdirectory(sample_fsim_payload)
add_subdirectory(counters)
if(KEDR_CALLM_PAYLOADS)
    add_subdirectory(custom_payload_callm)
endif(KEDR_CALLM_PAYLOADS)
add_subdirectory(custom_payload_fsim)
add_subdirectory(custom_indicator_fsim)

if(KEDR_LEAK_CHECK)
    add_subdirectory(leak_check)
endif(KEDR_LEAK_CHECK) 
